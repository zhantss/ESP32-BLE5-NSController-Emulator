#!/usr/bin/env python3
"""
Script for packaging firmware for ESP32-BLE5-NSController-Emulator.
"""

import os
import json
import sys
import subprocess
import re


def get_idf_target():
    """
    Read IDF_TARGET value from build/config.env JSON file.

    Returns:
        str: The IDF_TARGET value (e.g., "esp32c61") or None if not found.
    """
    # Determine the path to config.env relative to this script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)  # go up one level from scripts/
    config_path = os.path.join(project_root, "build", "config.env")

    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            config_data = json.load(f)
    except FileNotFoundError:
        print(f"Error: config.env file not found at {config_path}", file=sys.stderr)
        return None
    except json.JSONDecodeError as e:
        print(f"Error: Failed to parse JSON in {config_path}: {e}", file=sys.stderr)
        return None
    except Exception as e:
        print(f"Error: Unexpected error reading {config_path}: {e}", file=sys.stderr)
        return None

    idf_target = config_data.get("IDF_TARGET")
    if idf_target is None:
        print(f"Error: IDF_TARGET key not found in {config_path}", file=sys.stderr)
        return None

    return idf_target


def extract_flash_size_from_args(flash_args_lines):
    """
    Extract flash size from flash_args lines (e.g., from '--flash_size 8MB').

    Args:
        flash_args_lines (list): List of strings from flash_args file.

    Returns:
        int or None: Flash size in MB as integer, or None if not found.
    """
    for line in flash_args_lines:
        # Look for --flash_size parameter
        match = re.search(r'--flash_size\s+(\d+)(MB|M|mb|m)', line, re.IGNORECASE)
        if match:
            try:
                return int(match.group(1))
            except ValueError:
                pass
    return None


def read_flash_args(build_dir):
    """
    Read flash_args file and return its content as list of lines.

    Args:
        build_dir (str): Path to build directory.

    Returns:
        list: List of strings (lines) from flash_args file, or None on error.
    """
    flash_args_path = os.path.join(build_dir, "flash_args")
    try:
        with open(flash_args_path, 'r', encoding='utf-8') as f:
            lines = [line.strip() for line in f.readlines() if line.strip()]
        return lines
    except FileNotFoundError:
        print(f"Error: flash_args file not found at {flash_args_path}", file=sys.stderr)
        return None
    except Exception as e:
        print(f"Error: Failed to read flash_args file: {e}", file=sys.stderr)
        return None


def package_firmware(output_dir=None):
    """
    Package firmware using esptool.

    Args:
        output_dir (str, optional): Directory to save output file.
            If None, uses project_root/release.

    Returns:
        str or None: Path to created firmware file, or None on error.
    """
    # Get project paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    build_dir = os.path.join(project_root, "build")

    # Get IDF_TARGET
    idf_target = get_idf_target()
    if not idf_target:
        print("Error: Cannot proceed without IDF_TARGET", file=sys.stderr)
        return None

    # Read flash_args
    flash_args_lines = read_flash_args(build_dir)
    if not flash_args_lines:
        return None

    # Extract flash size
    flash_size_mb = extract_flash_size_from_args(flash_args_lines)
    if flash_size_mb is None:
        print("Error: Could not extract flash size from flash_args", file=sys.stderr)
        return None

    # Determine output directory
    if output_dir is None:
        output_dir = os.path.join(project_root, "release")

    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)

    # Construct output filename
    output_filename = f"ns-controller-{idf_target}n{flash_size_mb}.bin"
    output_path = os.path.join(output_dir, output_filename)

    # Build esptool command
    cmd = ["esptool", "--chip", idf_target, "merge_bin"]

    # Add flash parameters from first line (if present)
    if flash_args_lines:
        # First line contains flash parameters
        flash_params = flash_args_lines[0].split()
        cmd.extend(flash_params)

    # Add output argument
    cmd.extend(["-o", output_path])

    # Add offset+file pairs from remaining lines
    for line in flash_args_lines[1:]:
        if line.strip():  # Skip empty lines
            cmd.extend(line.split())

    print(f"Packaging firmware for {idf_target} ({flash_size_mb}MB)...")
    print(f"Command: {' '.join(cmd)}")
    print(f"Output: {output_path}")

    # Execute esptool command
    try:
        # Run from build directory so relative paths in flash_args work
        result = subprocess.run(cmd, cwd=build_dir, capture_output=True, text=True, check=False)

        if result.returncode != 0:
            print(f"Error: esptool failed with return code {result.returncode}", file=sys.stderr)
            print(f"stderr: {result.stderr}", file=sys.stderr)
            return None

        print(f"Successfully created firmware: {output_path}")
        if result.stdout:
            print(f"esptool output: {result.stdout}")

        return output_path

    except FileNotFoundError:
        print("Error: esptool not found. Make sure esptool is installed and in PATH.", file=sys.stderr)
        return None
    except Exception as e:
        print(f"Error: Failed to execute esptool: {e}", file=sys.stderr)
        return None


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Package firmware for ESP32-BLE5-NSController-Emulator")
    parser.add_argument("-o", "--output-dir", help="Output directory for firmware file")
    parser.add_argument("--test-only", action="store_true", help="Only test IDF_TARGET extraction, don't package")

    args = parser.parse_args()

    if args.test_only:
        # Test IDF_TARGET extraction
        target = get_idf_target()
        if target:
            print(f"IDF_TARGET: {target}")

            # Test flash_args reading
            script_dir = os.path.dirname(os.path.abspath(__file__))
            project_root = os.path.dirname(script_dir)
            build_dir = os.path.join(project_root, "build")
            flash_args_lines = read_flash_args(build_dir)

            if flash_args_lines:
                print(f"flash_args lines: {len(flash_args_lines)}")
                flash_size_mb = extract_flash_size_from_args(flash_args_lines)
                print(f"Flash size: {flash_size_mb}MB")

                # Show expected filename
                output_filename = f"ns-controller-{target}n{flash_size_mb}.bin"
                print(f"Expected output filename: {output_filename}")
            else:
                print("Failed to read flash_args")
                sys.exit(1)
        else:
            print("Failed to get IDF_TARGET")
            sys.exit(1)
    else:
        # Package firmware
        firmware_path = package_firmware(args.output_dir)
        if not firmware_path:
            sys.exit(1)