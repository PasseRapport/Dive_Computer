# SPDX-License-Identifier: GPL-3.0-or-later
"""Mechanical decoding of generated Devicetree artifacts for acceptance scripts.

Independent pin, address and electrical expectations belong in each caller.
"""
import struct


def cells(prop):
    """Decode resolved big-endian cells, including dtlib phandle markers."""
    assert len(prop.value) % 4 == 0, prop.name
    return list(struct.unpack('>' + 'I' * (len(prop.value) // 4), prop.value))


def enabled(node):
    """A node is enabled only when it and all ancestors have no status or okay."""
    if node is None:
        return False
    while node is not None:
        if 'status' in node.props and node.props['status'].to_string() != 'okay':
            return False
        node = node.parent
    return True
