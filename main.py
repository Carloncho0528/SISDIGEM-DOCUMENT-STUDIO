"""
==============================================================
 SISDIGEM DOCUMENT STUDIO
--------------------------------------------------------------

Version : 0.1
Module  : HTML to CSV Converter

Description
-----------
This is the first prototype of SISDIGEM Document Studio.

The objective of this script is to extract messages from an
exported HTML document and save them into a CSV file for
further analysis.

Author:
Carlos Arturo Garzón
SISDIGEM - Engineering Software for Real-World Problems

GitHub:
https://github.com/Carloncho0528

==============================================================
"""

from bs4 import BeautifulSoup
import pandas as pd
from pathlib import Path


# ============================================================
# Configuration
# ============================================================

INPUT_FILE = "messages.html"
OUTPUT_FILE = "messages.csv"


# ============================================================
# Main Function
# ============================================================

def main():

    print("=" * 60)
    print("SISDIGEM DOCUMENT STUDIO")
    print("Version 0.1")
    print("HTML -> CSV Converter")
    print("=" * 60)

    html_path = Path(INPUT_FILE)

    if not html_path.exists():
        print(f"\nERROR: File '{INPUT_FILE}' was not found.")
        return

    print("\nLoading HTML file...")

    try:

        with open(html_path, "r", encoding="utf-8") as file:
            soup = BeautifulSoup(file, "html.parser")

        print("Extracting messages...")

        data = []

        messages = soup.find_all(
            "div",
            class_="message default clearfix"
        )

        for msg in messages:

            # User name
            name_tag = msg.find("div", class_="from_name")
            name = name_tag.text.strip() if name_tag else "System"

            # Date / Time
            date_tag = msg.find(
                "div",
                class_="pull_right date details"
            )

            time = date_tag.text.strip() if date_tag else "N/A"

            # Message
            text_tag = msg.find("div", class_="text")
            text = text_tag.text.strip() if text_tag else ""

            data.append({
                "Usuario": name,
                "Hora": time,
                "Mensaje": text
            })

        df = pd.DataFrame(data)

        df.to_csv(
            OUTPUT_FILE,
            index=False,
            encoding="utf-8-sig"
        )

        print("\nProcess completed successfully!")
        print(f"Messages extracted : {len(df)}")
        print(f"Output file        : {OUTPUT_FILE}")

    except Exception as e:

        print("\nUnexpected error:")
        print(e)


# ============================================================
# Program Entry Point
# ============================================================

if __name__ == "__main__":
    main()
