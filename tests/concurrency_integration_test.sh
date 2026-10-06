#!/bin/sh

executor_pgid=""
launcher_pid=""
test_dir=""

wait_for_file() {
    file_path="$1"
    count=0

    while [ "$count" -lt 100 ]; do
        if [ -e "$file_path" ]; then
            return 0
        fi

        sleep 0.1
        count=$((count+1))
    done

    return 1
}

wait_for_nonempty_file() {
    file_path="$1"
    count=0

    while [ "$count" -lt 100 ]; do
        if [ -s "$file_path" ]; then
            return 0
        fi

        sleep 0.1
        count=$((count+1))
    done

    return 1
}

release_gate() {
    gate_path="$1"

    if ! timeout 2 /bin/sh -c '
        printf "go\n" > "$1"
    ' sh "$gate_path"; then
        return 1
    fi

    return 0
}

cleanup() {
    if [ -n "$executor_pgid" ]; then
        kill -- "-$executor_pgid" 2>/dev/null || true
    fi

    if [ -n "$launcher_pid" ]; then
        wait "$launcher_pid" 2>/dev/null || true
    fi

    rm -rf "$test_dir"
}

trap cleanup EXIT

if ! command -v setsid >/dev/null 2>&1; then
    echo "FAIL: setsid is required for this integration test"
    exit 1
fi

if [ "$#" -ne 2 ]; then
    echo "FAIL: expected work_executor path and manifest path"
    exit 1
fi

work_executor="$1"
manifest_path="$2"

if [ ! -x "$work_executor" ]; then
    echo "FAIL: work_executor is not executable: $work_executor"
    exit 1
fi

if [ ! -f "$manifest_path" ]; then
    echo "FAIL: manifest file does not exist: $manifest_path"
    exit 1
fi

test_dir=$(mktemp -d)

if [ -z "$test_dir" ]; then
    echo "FAIL: could not create temporary directory"
    exit 1
fi

gate0="$test_dir/test_gate0"
gate1="$test_dir/test_gate1"

j0_started="$test_dir/J0_started"
j0_done="$test_dir/J0_done"

j1_started="$test_dir/J1_started"
j1_done="$test_dir/J1_done"

j2_started="$test_dir/J2_started"
j2_done="$test_dir/J2_done"

executor_pgid_file="$test_dir/executor_pgid_file"

if ! mkfifo "$gate0"; then
    echo "FAIL: could not create gate0"
    exit 1
fi

if ! mkfifo "$gate1"; then
    echo "FAIL: could not create gate1"
    exit 1
fi

setsid --fork --wait /bin/sh -c '
    echo $$ > "$3" || exit 125
    cd "$4" || exit 126
    exec "$1" "$2" --concurrency 2
' sh "$work_executor" "$manifest_path" "$executor_pgid_file" "$test_dir" &

launcher_pid=$!

if ! wait_for_nonempty_file "$executor_pgid_file"; then
    echo "FAIL: time out waiting for executor process group"
    exit 1
fi

executor_pgid=$(cat "$executor_pgid_file")

if [ -z "$executor_pgid" ]; then
    echo "FAIL: executor PGID file was empty"
    exit 1
fi

if ! wait_for_file "$j0_started"; then
    echo "FAIL: time out waiting for J0_started"
    exit 1
fi

if ! wait_for_file "$j1_started"; then
    echo "FAIL: time out waiting for J1_started"
    exit 1
fi

if ! release_gate "$gate1"; then
    echo "FAIL: could not release gate1"
    exit 1
fi

if ! wait_for_file "$j1_done"; then
    echo "FAIL: time out waiting for J1_done"
    exit 1
fi

if ! wait_for_file "$j2_started"; then
    echo "FAIL: time out waiting for J2_started"
    exit 1
fi

if [ -e "$j0_done" ]; then
    echo "FAIL: J0 completed before gate0 was released"
    exit 1
fi

if ! wait_for_file "$j2_done"; then
    echo "FAIL: time out waiting for J2_done"
    exit 1
fi

if ! release_gate "$gate0"; then
    echo "FAIL: could not release gate0"
    exit 1
fi

if ! wait_for_file "$j0_done"; then
    echo "FAIL: time out waiting for J0_done"
    exit 1
fi

if ! wait "$launcher_pid"; then
    echo "FAIL: work_executor exited with failure"
    exit 1
fi

executor_pgid=""
launcher_pid=""

echo "PASS"
exit 0
