#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <fstream>
#include <getopt.h>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <secp256k1.h>
#include <cstring>   // for strchr
#include <algorithm> // for std::find_if

std::atomic<bool> found(false);
std::atomic<size_t> attempts(0);
std::mutex output_mutex;

constexpr const char *base58_chars = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
constexpr size_t base58_len = 58;

// ---------------------- Base58Check Decode ----------------------

bool DecodeBase58Check(const std::string &str, std::vector<unsigned char> &out)
{
    std::vector<unsigned char> b256((str.size() * 733 + 999) / 1000);
    std::fill(b256.begin(), b256.end(), 0);

    for (char ch : str)
    {
        const char *p = std::strchr(base58_chars, ch);
        if (!p)
            return false;
        int carry = p - base58_chars;

        for (auto it = b256.rbegin(); it != b256.rend(); ++it)
        {
            carry += 58 * (*it);
            *it = carry % 256;
            carry /= 256;
        }
        if (carry != 0)
            return false;
    }

    size_t leading_zeros = 0;
    while (leading_zeros < str.size() && str[leading_zeros] == '1')
        leading_zeros++;

    auto it = std::find_if(b256.begin(), b256.end(), [](unsigned char c)
                           { return c != 0; });
    out.assign(leading_zeros, 0x00);
    out.insert(out.end(), it, b256.end());

    if (out.size() < 4)
        return false;

    unsigned char hash1[SHA256_DIGEST_LENGTH], hash2[SHA256_DIGEST_LENGTH];
    SHA256(out.data(), out.size() - 4, hash1);
    SHA256(hash1, SHA256_DIGEST_LENGTH, hash2);

    if (!std::equal(out.end() - 4, out.end(), hash2))
        return false;

    out.resize(out.size() - 4);
    return true;
}

// ---------------------- Base58Check Encode ----------------------

std::string EncodeBase58Check(const std::vector<unsigned char> &input)
{
    std::vector<unsigned char> data = input;
    unsigned char hash1[SHA256_DIGEST_LENGTH], hash2[SHA256_DIGEST_LENGTH];

    SHA256(data.data(), data.size(), hash1);
    SHA256(hash1, SHA256_DIGEST_LENGTH, hash2);

    data.insert(data.end(), hash2, hash2 + 4);

    std::vector<unsigned char> b58((data.size() * 138 / 100) + 1);
    size_t length = 0;

    for (unsigned char byte : data)
    {
        int carry = byte;
        size_t i = 0;

        for (auto it = b58.rbegin(); (carry != 0 || i < length) && it != b58.rend(); ++it, ++i)
        {
            carry += 256 * (*it);
            *it = carry % 58;
            carry /= 58;
        }
        length = i;
    }

    auto it = std::find_if(b58.begin(), b58.end(), [](unsigned char c)
                           { return c != 0; });

    std::string result;

    for (unsigned char byte : data)
    {
        if (byte == 0x00)
            result += '1';
        else
            break;
    }

    for (; it != b58.end(); ++it)
        result += base58_chars[*it];

    return result;
}

// ---------------------- Address Generation ----------------------

std::string generate_address_from_wif(const std::string &wif)
{
    std::vector<unsigned char> decoded;

    if (!DecodeBase58Check(wif, decoded))
        return "";

    bool compressed = (decoded.size() == 34 && decoded.back() == 0x01);

    if (compressed)
        decoded.pop_back();

    if (decoded.size() != 33)
        return "";

    std::vector<unsigned char> privkey(decoded.begin() + 1, decoded.end());

    secp256k1_context *ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);

    secp256k1_pubkey pubkey;

    if (!secp256k1_ec_pubkey_create(ctx, &pubkey, privkey.data()))
    {
        secp256k1_context_destroy(ctx);
        return "";
    }

    unsigned char serialized_pubkey[65];
    size_t pubkey_len = compressed ? 33 : 65;

    secp256k1_ec_pubkey_serialize(
        ctx,
        serialized_pubkey,
        &pubkey_len,
        &pubkey,
        compressed ? SECP256K1_EC_COMPRESSED : SECP256K1_EC_UNCOMPRESSED);

    secp256k1_context_destroy(ctx);

    unsigned char sha256_hash[SHA256_DIGEST_LENGTH];
    SHA256(serialized_pubkey, pubkey_len, sha256_hash);

    unsigned char ripemd160_hash[RIPEMD160_DIGEST_LENGTH];
    RIPEMD160(sha256_hash, SHA256_DIGEST_LENGTH, ripemd160_hash);

    std::vector<unsigned char> address = {0x00};
    address.insert(address.end(), ripemd160_hash, ripemd160_hash + RIPEMD160_DIGEST_LENGTH);

    return EncodeBase58Check(address);
}

// ---------------------- Worker Thread ----------------------

void brute_force_worker(const std::string &template_wif,
                        const std::vector<size_t> &positions,
                        const std::string &target_address,
                        size_t thread_index, size_t total_threads,
                        size_t total, size_t resume_offset)
{
    size_t pos_count = positions.size();

    for (size_t idx = resume_offset + thread_index;
         idx < total && !found.load();
         idx += total_threads)
    {
        std::string candidate(template_wif);
        size_t n = idx;

        for (size_t i = 0; i < pos_count; ++i)
        {
            candidate[positions[i]] = base58_chars[n % base58_len];
            n /= base58_len;
        }

        attempts++;

        std::string generated = generate_address_from_wif(candidate);

        if (!generated.empty() && generated == target_address)
        {
            if (!found.exchange(true))
            {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cout << "\n\n🔥 FOUND WIF: " << candidate << "\n";

                std::ofstream out("KEYFOUND.txt", std::ios::app);
                out << candidate << "\n";
            }
            return;
        }
    }
}

// ---------------------- MAIN ----------------------

int main(int argc, char *argv[])
{
    std::string address, partial_wif, resume_file;

    int threads = std::thread::hardware_concurrency();
    int progress_interval = 10;

    int opt;

    while ((opt = getopt(argc, argv, "a:k:t:p:r:")) != -1)
    {
        switch (opt)
        {
        case 'a':
            address = optarg;
            break;
        case 'k':
            partial_wif = optarg;
            break;
        case 't':
            threads = std::stoi(optarg);
            break;
        case 'p':
            progress_interval = std::stoi(optarg);
            break;
        case 'r':
            resume_file = optarg;
            break;
        default:
            std::cerr << "Usage: " << argv[0]
                      << " -a <address> -k <partial_wif> [-t threads] [-p interval] [-r resume_file]\n";
            return 1;
        }
    }

    if (address.empty() || partial_wif.empty())
    {
        std::cerr << "❌ Address and partial WIF must be provided.\n";
        return 1;
    }

    // Identify wildcard locations
    std::vector<size_t> wildcard_positions;

    for (size_t i = 0; i < partial_wif.size(); ++i)
        if (partial_wif[i] == '_')
            wildcard_positions.push_back(i);

    if (wildcard_positions.empty())
    {
        std::cerr << "❌ No '_' wildcards found in WIF.\n";
        return 1;
    }

    // Count combinations
    size_t total_combinations = 1;

    for (size_t i = 0; i < wildcard_positions.size(); ++i)
        total_combinations *= base58_len;

    // ---------------------- Resume Support ----------------------

    size_t resume_start_index = 0;

    if (!resume_file.empty())
    {
        std::ifstream in(resume_file);
        if (in.is_open())
        {
            in >> resume_start_index;
            std::cout << "⏯️  Resuming from: " << resume_start_index << "\n";
        }
        std::ofstream(resume_file, std::ios::app).close();
    }

    std::cout << "🔍 Total combinations: " << total_combinations << "\n";
    std::cout << "🧵 Using threads: " << threads << "\n";

    // Progress thread
    auto start_time = std::chrono::steady_clock::now();
    size_t last_saved = 0;

    std::thread progress([&]()
                         {
        while (!found.load()) {

            std::this_thread::sleep_for(std::chrono::seconds(progress_interval));
            if (found.load()) break;

            size_t current = resume_start_index + attempts.load();
            double percent = (100.0 * current) / total_combinations;

            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - start_time).count();
            double rate = current / elapsed;

            double eta = (rate > 0) ? (total_combinations - current) / rate : 0;

            std::string bar = std::string(int(percent / 2), '#');
            bar.resize(50, '.');

            std::lock_guard<std::mutex> lock(output_mutex);
            std::cout << "\r\033[K[" << bar << "] "
                      << int(percent) << "%  ("
                      << current << "/" << total_combinations
                      << ")  ETA: " << int(eta) << "s   " << std::flush;

            // Save progress
            if (!resume_file.empty() && current > last_saved + 100000) {
                std::ofstream out(resume_file);
                out << current << "\n";
                last_saved = current;
            }
        } });

    // ---------------------- Worker threads ----------------------

    std::vector<std::thread> pool;

    for (int i = 0; i < threads; ++i)
    {
        pool.emplace_back(
            brute_force_worker,
            partial_wif,
            wildcard_positions,
            address,
            i,
            threads,
            total_combinations,
            resume_start_index);
    }

    for (auto &t : pool)
        t.join();

    progress.join();

    auto end_time = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();

    std::cout << "\n\n⏱️  Time: " << elapsed << " sec\n";
    std::cout << "⚡ Speed: " << size_t(attempts / elapsed) << " keys/sec\n";

    if (found && !resume_file.empty())
        std::remove(resume_file.c_str());

    return 0;
}
