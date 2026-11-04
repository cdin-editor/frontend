# Terminal layout (planned)

Map the shared logical pane tree and text positions to terminal rows and cells.
This adapter owns cell width, tabs, terminal wrapping, viewport and visual
cursor placement; it never writes cell/pixel coordinates to the Protocol.
