#!/usr/bin/env python3

import os
import json
import glob
import sys


def find_firmware_name(endswith=".bin"):
    firmware_directory = "web/firmware/"
    firmware_files = glob.glob(os.path.join(firmware_directory, f"*{endswith}"))
    if firmware_files:
        return os.path.splitext(os.path.basename(firmware_files[0]))[0]
    return None


def fill_manifest(github_username, github_project, github_build_number):
    # Check for ESP32 firmware
    esp32_firmware = find_firmware_name("esp32.bin")
    esp32s3_firmware = find_firmware_name("esp32s3.bin")

    if not esp32_firmware and not esp32s3_firmware:
        print(
            "ERROR: No firmware files found in 'web/firmware/' directory.",
            file=sys.stderr,
        )
        print(
            "Expected files matching patterns: *esp32.bin or *esp32s3.bin",
            file=sys.stderr,
        )
        raise ValueError(
            "No firmware files found in the 'web/firmware/' directory with '.bin' extension."
        )

    builds = []

    if esp32_firmware:
        builds.append(
            {
                "chipFamily": "ESP32",
                "parts": [{"path": f"firmware/{esp32_firmware}.bin", "offset": 0}],
            }
        )
        print(f"Found ESP32 firmware: {esp32_firmware}.bin")

    if esp32s3_firmware:
        builds.append(
            {
                "chipFamily": "ESP32-S3",
                "parts": [{"path": f"firmware/{esp32s3_firmware}.bin", "offset": 0}],
            }
        )
        print(f"Found ESP32-S3 firmware: {esp32s3_firmware}.bin")

    return {
        "name": f"{github_username}/{github_project}",
        "version": f"0.0.{github_build_number}",
        "funding_url": f"https://github.com/{github_username}/{github_project}/contributors",
        "builds": builds,
    }


if __name__ == "__main__":
    # Retrieve values from GitHub Actions environment variables
    # Use fallback values for local development
    github_repo = os.environ.get("GITHUB_REPOSITORY", "n0rule/LoveSpouse-Gateway")
    github_username, github_project = github_repo.split("/")
    github_build_number = os.environ.get("GITHUB_RUN_NUMBER", "dev")

    try:
        manifest_data = fill_manifest(
            github_username, github_project, github_build_number
        )

        output_file = "web/esp-webtools-manifest.json"

        with open(output_file, "w") as file:
            json.dump(manifest_data, file, indent=2)

        print(f"✓ Manifest has been filled and saved as {output_file}")
        print(f"  Project: {github_username}/{github_project}")
        print(f"  Version: 0.0.{github_build_number}")
    except Exception as e:
        print(f"ERROR: Failed to generate manifest: {e}", file=sys.stderr)
        sys.exit(1)
