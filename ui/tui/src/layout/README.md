# Terminal layout

The TUI adapter parses and retains the Protocol 1.0.0 PaneTree as bounded
semantic state. It validates nullable root/active leaf, unique node and view
IDs, leaf view membership, nullable active view, split axis and fixed-point
ratio. The parsed tree is staged with the atomic UI snapshot and becomes
visible through the model accessor only after `ui.flush` commits.

The parser does not calculate terminal rectangles or render multiple panes
yet. This adapter owns the future cell mapping, tabs, wrapping, viewport and
visual cursor placement; it never writes cell/pixel coordinates to Protocol.
