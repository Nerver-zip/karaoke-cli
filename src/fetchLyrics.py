import os
import requests
import sys
import urllib.parse

def baixar_lrc(track, artist):
    base_url = "https://lrclib.net/api/get"
    params = {
        "track_name": track,
        "artist_name": artist
    }
    url = f"{base_url}?{urllib.parse.urlencode(params)}"
    print(f"🔍 Searching lyrics: {track} - {artist}")

    try:
        response = requests.get(url, timeout=10)
        response.raise_for_status()
        data = response.json()
    except Exception as e:
        print("❌ Request error:", e)
        return

    base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../local/songs"))
    os.makedirs(base_dir, exist_ok=True)

    if data.get("syncedLyrics"):
        lrc_content = data["syncedLyrics"]
        filename = os.path.join(base_dir, f"{track} - {artist}.lrc")
        with open(filename, "w", encoding="utf-8") as f:
            f.write(lrc_content)
        print(f"✅ Synced lyrics saved in '{filename}'")

    elif data.get("plainLyrics"):
        filename = os.path.join(base_dir, f"{track} - {artist}.lrc")  # usa .lrc também
        with open(filename, "w", encoding="utf-8") as f:
            f.write(data["plainLyrics"])
        print(f"⚠️ Unsynced lyrics saved in '{filename}'")

    else:
        print("🚫 No lyrics found.")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Uso: python3 baixar_lrc.py \"Nome da música\" \"Artista\"")
    else:
        track = sys.argv[1]
        artist = sys.argv[2]
        baixar_lrc(track, artist)
