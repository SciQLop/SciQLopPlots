#!/bin/sh
HERE=$(dirname $0)
INCLUDE_DIR=$HERE/../../include/

cleanup_str() {
    echo "$1" | tr -d " " | tr -d "\n" | tr -d "\t"
}

search_for_signal(){
    for file in $1/*; do
        if [ -d $file ]; then
            search_for_signal $file
        else
            HAS_SIGNAL=$(cat $file | sed '/\#ifndef BINDINGS_H/,/\#endif \/\/ BINDINGS_H/d' | grep "signal:\|Q_SIGNAL")
            if [ $(cleanup_str "$HAS_SIGNAL") ]; then
                echo "$file : $HAS_SIGNAL"
            fi
        fi
    done
}

USES_EMIT_IN_HEADERS=$(grep -R " emit " $INCLUDE_DIR)

if [ $(cleanup_str "$USES_EMIT_IN_HEADERS") ]; then
    echo "FAIL: emit keyword used in headers"
    echo "$USES_EMIT_IN_HEADERS"
    exit 1
fi

USES_CONNECT_IN_HEADERS=$(grep -R " connect(" $INCLUDE_DIR | grep -v "#SciQLop-check-ignore-connect")

if [ $(cleanup_str "$USES_CONNECT_IN_HEADERS") ]; then
    echo "FAIL: connect() function used in headers"
    echo "$USES_CONNECT_IN_HEADERS"
    exit 1
fi

USES_SIGNAL_IN_HEADERS=$(search_for_signal $INCLUDE_DIR)

if [ $(cleanup_str "$USES_SIGNAL_IN_HEADERS") ]; then
    echo "FAIL: Signal used in headers"
    echo "$USES_SIGNAL_IN_HEADERS"
    exit 1
fi

echo "PASS"
exit 0
