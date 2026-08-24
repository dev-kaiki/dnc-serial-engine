v14c fixes

- Removed hidden initial-XON startup gate in FlowController.
- Settings no longer persist requireXonToSend from hidden UI.
- MachineConfigRepository now restores framing fields introduced in v10+.
- Handshake monitor only polls in CTS/RTS mode.
