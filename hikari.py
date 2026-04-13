import os
import sys
import time
import random
import threading
import subprocess
import requests
from pathlib import Path

MAX_FOLDERS = 12
FILES_PER_FOLDER = 5
INFO = "\033[37m[\033[31m+\033[37m]\033[33m"
ASK = "\033[33m[?]\033[37m"
ERROR = "\033[31m[!] \033[41m\033[30mERROR\033[40m\033[37m"
RESET = "\033[0m"

class EditorStatus:
    def __init__(self):
        self.total_processed = 0
        self.current_folder = 1
        self.current_file_idx = 0
        self.is_running = True
        self.current_percent = 0

def get_signal_display(kb):
    if kb == 0: return "\033[35m"
    if kb < 50: return "\033[90m"
    if kb < 150: return "\033[31m"
    if kb < 500: return "\033[33m"
    return "\033[32m"

def editor_animator(status):
    banner = "Hikari video editor with ffmpeg"
    idx = 0
    while status.is_running:
        animated_banner = "".join([
            char.upper() if i == idx else char.lower() 
            for i, char in enumerate(banner)
        ])
        sys.stdout.write(
            f"\r\033[K[{status.total_processed}/60 | {status.current_percent}% | "
            f"F{status.current_folder:02d}-V{status.current_file_idx}] \033[36m{animated_banner}{RESET}"
        )
        sys.stdout.flush()
        idx = (idx + 1) % len(banner)
        time.sleep(0.2)

def process_video(input_file, output_file, gif, png, duration, status):
    filter_complex = (
        f"[0:v]scale=720:480:force_original_aspect_ratio=decrease,pad=720:480:(720-iw)/2:(480-ih)/2,setsar=1[main];"
        f"[1:v]scale=720:40[top];[2:v]scale=720:40[bottom];[3:v]scale=100:-1,format=rgba,colorchannelmixer=aa=0.8[logo];"
        f"[main][top]overlay=0:0:shortest=1[v1];[v1][bottom]overlay=0:H-h:shortest=1[v2];[v2][logo]overlay=20:50"
    )

    cmd = [
        'ffmpeg', '-y', '-i', input_file, '-t', str(duration),
        '-ignore_loop', '0', '-i', gif, '-ignore_loop', '0', '-i', gif, '-i', png,
        '-filter_complex', filter_complex,
        '-c:v', 'libx264', '-preset', 'veryfast', '-crf', '28', 
        '-pix_fmt', 'yuv420p', '-c:a', 'aac', '-b:a', '64k', '-shortest', output_file
    ]

    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    for line in process.stdout:
        if "time=" in line:
            try:
                time_str = line.split("time=")[1].split()[0]
                h, m, s = time_str.split(':')
                current_secs = int(h)*3600 + int(m)*60 + float(s)
                status.current_percent = min(100, int((current_secs * 100) / duration))
            except:
                pass
    process.wait()
    status.current_percent = 0

def video_editor(gif_path, png_path, target_dir, duration):
    if not os.path.exists(target_dir):
        print(f"\n{ERROR} Directory video tidak ditemukan!{RESET}")
        return

    abs_gif = os.path.abspath(gif_path)
    abs_png = os.path.abspath(png_path)
    status = EditorStatus()
    anim_thread = threading.Thread(target=editor_animator, args=(status,))
    anim_thread.start()

    os.chdir(target_dir)
    files = [f for f in os.listdir('.') if f.endswith('.mp4') and not f.startswith('edit_')]
    for file in files:
        if status.total_processed >= 60: break

        folder_name = f"edit_{status.current_folder}"
        if not os.path.exists(folder_name):
            os.makedirs(folder_name)

        status.current_file_idx += 1
        if status.current_file_idx > FILES_PER_FOLDER:
            status.current_file_idx = 1
            status.current_folder += 1

        output_path = os.path.join(folder_name, f"edit_{status.current_file_idx}.mp4")
        process_video(file, output_path, abs_gif, abs_png, duration, status)
        status.total_processed += 1

    status.is_running = False
    anim_thread.join()
    print(f"\n\033[32m[+] Batch Rendering Selesai.{RESET}")

def is_blacklisted(url, path):
    if not os.path.exists(path): return False
    with open(path, 'r') as f:
        return url in f.read()

def scanner_animator(data):
    banner = "Hikari-ng Eporner scanner"
    idx = 0
    while data['is_running']:
        kb = random.randint(0, 1200)
        color = get_signal_display(kb)
        animated_banner = "".join([
            char.upper() if i == idx else char.lower() 
            for i, char in enumerate(banner)
        ])
        sys.stdout.write(
            f"\r\033[K[{data['total']}/{data['target']} | {data['user']} | {color}{kb:4d} KBps{RESET}] "
            f"\033[36m{animated_banner}{RESET}"
        )
        sys.stdout.flush()
        idx = (idx + 1) % len(banner)
        time.sleep(0.2)

def link_scanner(username, target_link, blacklist_path):
    data = {
        'total': 0,
        'target': target_link,
        'user': username,
        'is_running': True
    }
    anim_thread = threading.Thread(target=scanner_animator, args=(data,))
    anim_thread.start()

    target_url = f"https://www.eporner.com/profile/{username}/"
    headers = {'User-Agent': 'Hikari-Scanner/7.0'}

    try:
        while data['is_running'] and data['total'] < target_link:
            try:
                response = requests.get(target_url, headers=headers, timeout=10)
                if response.status_status == 200:
                    import re
                    links = re.findall(r'href="(/video-[^"]+)"', response.text)
                    for path in links:
                        full_url = f"https://www.eporner.com{path}"
                        if not is_blacklisted(full_url, blacklist_path):
                            sys.stdout.write(f"\r\033[K\033[32m{full_url}{RESET}\n")
                            with open(blacklist_path, 'a') as fb: fb.write(full_url + '\n')
                            with open('link.txt', 'a') as fl: fl.write(full_url + '\n')
                            data['total'] += 1
                            if data['total'] >= target_link: break
            except Exception:
                pass
            time.sleep(2)
    except KeyboardInterrupt:
        pass
    finally:
        data['is_running'] = False
        anim_thread.join()

def main():
    while True:
        print(f"\n\033[37m1. Edit Video (FFmpeg)\n2. Link Scanner (Eporner)\n3. Exit")
        choice = input(f"{ASK} Pilihan: ")

        if choice == '1':
            g = input(f"{ASK} Path GIF: ")
            i = input(f"{ASK} Path PNG: ")
            d = input(f"{ASK} Path Video Folder: ")
            try:
                dur = int(input(f"{ASK} Durasi (detik): "))
                video_editor(g, i, d, dur)
            except ValueError:
                print(f"{ERROR} Durasi harus angka!")

        elif choice == '2':
            u = input(f"{ASK} Username: ")
            try:
                t = int(input(f"{ASK} Target: "))
                b = input(f"{ASK} Blacklist Path: ")
                link_scanner(u, t, b)
            except ValueError:
                print(f"{ERROR} Target harus angka!")

        elif choice == '3':
            print("Exiting...")
            break

if __name__ == "__main__":
    print(f"{INFO} Starting Hikari-ng 7.0.0 (Python Edition)")
    try:
        main()
    except KeyboardInterrupt:
        print(f"{INFO} Papay!!")
        sys.exit()
