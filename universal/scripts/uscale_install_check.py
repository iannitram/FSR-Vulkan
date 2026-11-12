#!/usr/bin/env python3
"""uscale_install_check.py: verifies install tree contains no vendor binaries or models (SPEC M12).

Fails if the install tree contains any NVIDIA, AMD or Intel binary or model file.
SPDX-License-Identifier: MIT
"""
import argparse
import os
import re
import shutil
import sys
import tempfile

# Patterns that match forbidden vendor runtime binaries or model files
FORBIDDEN_RULES = [
    # NVIDIA runtimes & binaries
    (re.compile(r"(nvngx|nvidia[-_]ngx|nvsdk[-_]ngx)", re.IGNORECASE), "NVIDIA NVNGX/DLSS runtime"),
    # Intel XeSS runtimes & binaries
    (re.compile(r"libxess", re.IGNORECASE), "Intel XeSS runtime"),
    (re.compile(r"xess\.(dll|so|dylib|lib|a)", re.IGNORECASE), "Intel XeSS binary"),
    # AMD FidelityFX runtimes & binaries
    (re.compile(r"amd_fidelityfx", re.IGNORECASE), "AMD FidelityFX runtime"),
    # Machine learning / model weights and asset files
    (re.compile(r"\.(bin|weights|onnx|pb|tflite|safetensors|pt|engine)$", re.IGNORECASE), "Model or weight file"),
    (re.compile(r"model_v\d+", re.IGNORECASE), "Vendor model file"),
    (re.compile(r"(initializers|pre_weights)", re.IGNORECASE), "FSR model initializer/weights file"),
    (re.compile(r"\.spv$", re.IGNORECASE), "SPIR-V model/shader asset"),
]


def scan_install_tree(prefix):
    """Scans prefix and returns (scanned_count, violations)."""
    if not os.path.exists(prefix):
        return 0, [(prefix, f"Install directory does not exist: {prefix}")]

    scanned_count = 0
    violations = []

    for root, dirs, files in os.walk(prefix):
        for fname in files:
            scanned_count += 1
            full_path = os.path.join(root, fname)
            rel_path = os.path.relpath(full_path, prefix)
            base_name = os.path.basename(fname)
            for pattern, reason in FORBIDDEN_RULES:
                if pattern.search(base_name) or pattern.search(rel_path):
                    violations.append((rel_path, reason))
                    break

    return scanned_count, violations


def run_check(prefix):
    """Runs the install check on prefix. Returns 0 if clean, 1 if violations found."""
    scanned_count, violations = scan_install_tree(prefix)

    if violations:
        print(f"uscale_install_check: FAIL: {len(violations)} forbidden vendor or model file(s) found in '{prefix}':", file=sys.stderr)
        for rel_path, reason in violations:
            print(f"  - {rel_path} ({reason})", file=sys.stderr)
        return 1

    if scanned_count == 0:
        print(f"uscale_install_check: ERROR: install directory '{prefix}' contains no files", file=sys.stderr)
        return 1

    print(f"uscale_install_check: PASS (prefix '{prefix}': {scanned_count} files scanned, no vendor binaries or models)")
    return 0


def run_planted_tests(prefix):
    """Plants fake vendor files in temporary copies of prefix and asserts failure."""
    if not os.path.exists(prefix):
        print(f"uscale_install_check: ERROR: prefix '{prefix}' does not exist", file=sys.stderr)
        return 1

    fake_cases = [
        ("lib/nvngx_dlss.dll", "NVIDIA DLSS Windows runtime"),
        ("lib/libnvidia-ngx-dlss.so.1", "NVIDIA DLSS Linux runtime"),
        ("bin/libxess.dll", "Intel XeSS runtime"),
        ("lib/amd_fidelityfx_upscaler_vk.dll", "AMD FidelityFX runtime"),
        ("share/fsr4_model_v07_i8_weights.bin", "FSR 4 model weights"),
        ("share/fsr4_initializers.bin", "FSR 4 initializers"),
        ("models/upscaler.onnx", "ONNX model file"),
        ("shaders/fsr4_pass0.spv", "SPIR-V model shader asset"),
    ]

    print(f"uscale_install_check: running planted-file tests on copy of '{prefix}'...")
    with tempfile.TemporaryDirectory() as tmpdir:
        test_root = os.path.join(tmpdir, "prefix_copy")
        shutil.copytree(prefix, test_root)

        for rel_path, desc in fake_cases:
            target_path = os.path.join(test_root, rel_path)
            os.makedirs(os.path.dirname(target_path), exist_ok=True)
            with open(target_path, "wb") as f:
                f.write(b"FAKE_VENDOR_PAYLOAD\n")

            scanned, violations = scan_install_tree(test_root)
            os.remove(target_path)

            if not violations:
                print(f"uscale_install_check: planted test FAILED: check did not catch {rel_path} ({desc})", file=sys.stderr)
                return 1

            matched_files = [v[0] for v in violations]
            if rel_path not in matched_files:
                print(f"uscale_install_check: planted test FAILED: {rel_path} not among detected violations: {matched_files}", file=sys.stderr)
                return 1

    print(f"uscale_install_check: planted-file test PASS (all {len(fake_cases)} vendor/model cases correctly failed)")
    return 0


def main():
    parser = argparse.ArgumentParser(description="uscale install-tree vendor file check (SPEC M12)")
    parser.add_argument("prefix", help="Path to install prefix directory")
    parser.add_argument("--test-planted", action="store_true",
                        help="Run planted fake-file tests to prove check can fail")
    args = parser.parse_args()

    if args.test_planted:
        return run_planted_tests(args.prefix)
    return run_check(args.prefix)


if __name__ == "__main__":
    sys.exit(main())
