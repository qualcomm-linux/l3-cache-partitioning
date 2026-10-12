#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2024-2025, Qualcomm Innovation Center, Inc. All rights reserved.
#
# Smoke test for qcom-cache-partition module.
# Run as root on a QCS8300 or QCS9100 target with the module loaded.

set -e

PASS=0
FAIL=0
CONFIGFS_BASE=/sys/kernel/config/cache_partition

pass() { echo "PASS: $1"; PASS=$((PASS + 1)); }
fail() { echo "FAIL: $1"; FAIL=$((FAIL + 1)); }

# Prerequisites
[ "$(id -u)" = "0" ] || { echo "Must run as root"; exit 1; }

# Mount configfs if needed
mountpoint -q /sys/kernel/config || mount -t configfs none /sys/kernel/config

# Load module if not loaded
if ! lsmod | grep -q qcom_cache_partition; then
    modprobe qcom-cache-partition || { echo "Module load failed"; exit 1; }
fi

# Test 1: attributes exist
[ -f "${CONFIGFS_BASE}/partcr_el1" ]     && pass "partcr_el1 exists"     || fail "partcr_el1 missing"
[ -f "${CONFIGFS_BASE}/thread_sid_el1" ] && pass "thread_sid_el1 exists" || fail "thread_sid_el1 missing"

# Test 2: read returns output
cat "${CONFIGFS_BASE}/partcr_el1"     | grep -q "Cluster" && pass "partcr_el1 readable"     || fail "partcr_el1 not readable"
cat "${CONFIGFS_BASE}/thread_sid_el1" | grep -q "CPU"     && pass "thread_sid_el1 readable" || fail "thread_sid_el1 not readable"

# Test 3: write to cluster 0
echo "0 0" > "${CONFIGFS_BASE}/partcr_el1" && pass "partcr_el1 write cluster 0" || fail "partcr_el1 write cluster 0"

# Test 4: invalid format rejected
echo "zz xx" > "${CONFIGFS_BASE}/partcr_el1" 2>/dev/null && fail "bad format not rejected" || pass "bad format rejected"

# Test 5: offline CPU rejected
OFFLINE_CPU=$(cat /sys/devices/system/cpu/offline | cut -d, -f1 | cut -d- -f1)
if [ -n "$OFFLINE_CPU" ]; then
    echo "1 $OFFLINE_CPU" > "${CONFIGFS_BASE}/thread_sid_el1" 2>/dev/null \
        && fail "offline CPU not rejected" || pass "offline CPU rejected"
else
    echo "SKIP: no offline CPUs available"
fi

# Test 6: module unload/reload
modprobe -r qcom-cache-partition && pass "module unload" || fail "module unload"
modprobe qcom-cache-partition    && pass "module reload" || fail "module reload"

echo ""
echo "Results: ${PASS} passed, ${FAIL} failed"
[ "$FAIL" = "0" ]
