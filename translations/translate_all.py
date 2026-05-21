#!/usr/bin/env python3
import os
import glob
import urllib.request
import urllib.parse
import json
import time
import re

lang_map = {
    "cs": "cs",
    "da": "da",
    "es": "es",
    "fi": "fi",
    "fr": "fr",
    "hu": "hu",
    "it": "it",
    "ja": "ja",
    "ko": "ko",
    "nb": "no",
    "nl": "nl",
    "pl": "pl",
    "pt": "pt",
    "ro": "ro",
    "ru": "ru",
    "sk": "sk",
    "sv": "sv",
    "tr": "tr",
    "zh_CN": "zh-CN",
    "ar": "ar"
}

def translate_text(text, target_lang, source_lang="de"):
    if not text.strip():
        return ""
    # Google Translate client API
    url = "https://translate.googleapis.com/translate_a/single?client=gtx&sl=" + source_lang + "&tl=" + target_lang + "&dt=t&q=" + urllib.parse.quote(text)
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    for attempt in range(3):
        try:
            with urllib.request.urlopen(req, timeout=10) as response:
                data = json.loads(response.read().decode('utf-8'))
                translated = "".join([part[0] for part in data[0] if part[0] is not None])
                # Post-process placeholders
                translated = re.sub(r'%\s+(\d)', r'%\1', translated)
                return translated
        except Exception as e:
            print(f"  Attempt {attempt + 1} failed: {e}")
            time.sleep(1)
    return None

def escape_xml(text):
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;").replace("'", "&apos;")

def main():
    ts_files = glob.glob("/home/d.lange/SplitCommander/translations/*.ts")
    ts_files = [f for f in ts_files if "splitcommander_de.ts" not in f and "splitcommander_en.ts" not in f]

    print(f"Found {len(ts_files)} translation files to update.")

    for ts_path in sorted(ts_files):
        filename = os.path.basename(ts_path)
        lang_code = filename.replace("splitcommander_", "").replace(".ts", "")
        google_lang = lang_map.get(lang_code)

        if not google_lang:
            print(f"Skipping {filename}: Unknown language code mapping.")
            continue

        print(f"\nProcessing {filename} (Target: {google_lang})...")

        with open(ts_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # Regex to find all message blocks
        # <message> ... </message>
        message_pattern = re.compile(r'(<message>.*?</message>)', re.DOTALL)
        messages = message_pattern.findall(content)

        updated_count = 0
        new_content = content

        for msg in messages:
            # Check if translation is empty and unfinished
            if '<translation type="unfinished"></translation>' in msg:
                # Extract source text
                source_match = re.search(r'<source>(.*?)</source>', msg, re.DOTALL)
                if source_match:
                    source_text = source_match.group(1)
                    # Translate
                    translated = translate_text(source_text, google_lang)
                    if translated is not None:
                        escaped_translated = escape_xml(translated)
                        new_translation_tag = f"<translation>{escaped_translated}</translation>"
                        updated_msg = msg.replace('<translation type="unfinished"></translation>', new_translation_tag)
                        new_content = new_content.replace(msg, updated_msg)
                        updated_count += 1
                        # Sleep briefly to be respectful to the API
                        time.sleep(0.1)

        if updated_count > 0:
            with open(ts_path, 'w', encoding='utf-8') as f:
                f.write(new_content)
            print(f"  Successfully translated {updated_count} empty entries in {filename}!")
        else:
            print(f"  No empty entries to translate in {filename}.")

if __name__ == "__main__":
    main()
