# BTC Wif Recover

`recoverwif` is a multithreaded brute‑force tool to recover a Bitcoin WIF (Wallet Import Format) private key by filling in missing characters (underscores `_`) and comparing the derived address.

---

## Features

- Multithreaded brute forcing using all CPU cores  
- Resume support via progress file  
- Saves recovered key to KEYFOUND.txt  
- Works on Linux, macOS, and Windows  
- Shows live speed, ETA, and progress bar  
- Uses OpenSSL + libsecp256k1 for crypto  

---

## Requirements

You must have:

- OpenSSL (libssl + libcrypto)
- libsecp256k1
- g++ with C++17 support
- Make

```
git clone https://github.com/yourusername/recoverwif.git
cd recoverwif
```

### Macos
```
brew install openssl@3
```
```
brew install secp256k1
```
```
make mac
```

### Linux
```
sudo apt update
```

```
sudo apt install -y build-essential g++ make \
libssl-dev libsecp256k1-dev
```

```
make linux
```


### Windows
```
pacman -Syu
```

```
pacman -Su
```

```
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make \
mingw-w64-x86_64-openssl mingw-w64-x86_64-secp256k1
```
```
make win
```
---

## Building

This project includes a multi‑platform Makefile.

Build using:

Linux:
make linux

macOS:
make mac

Windows (MinGW):
make win

Clean:
make clean

---

## Usage

./recoverwif -a <address> -k <partial_wif> [-t threads] [-p progress_interval] [-r resume_file]

Options:

-a    Target Bitcoin address  
-k    Partial WIF, using "_" for unknown characters  
-t    Number of threads (default: all CPU cores)  
-p    Progress update interval in seconds (default: 10)  
-r    Resume file (saves progress and continues later)  

---

## Example
```
./recoverwif -a 13YdfiT6u1zNwZhzZCieKr7hBcZSiA6vqQ -k 5HpHagT65TZzG1PH3CSu63k8DbpvD8s5iujXoV3MpXFEEX_ro__ -t 8 -p 1 -r progress.txt
```

---

## Output

```
🔍 Total combinations: 195112
🧵 Using threads: 8


🔥 FOUND WIF: 5HpHagT65TZzG1PH3CSu63k8DbpvD8s5iujXoV3MpXFEEX9roUW


⏱️  Time: 1.00558 sec
⚡ Speed: 103370 keys/sec
```

---

## Resume Support

- You may stop the tool anytime (Ctrl+C).  
- When using -r progress.txt, it resumes scanning automatically.  
- Progress is updated every ~100k attempts.

---

## Notes

- Brute forcing WIF keys is computationally expensive.  
- Use ONLY for recovering your own keys.  
- Do not use this tool for anything illegal.  
- You are solely responsible for how you use this software. The authors and contributors take no liability for misuse.

---

## License

MIT License.
