# Temporary direct UWB pair diagnostic

This is a temporary hardware diagnostic approved on 2026-09-28. It replaces
the six-node mesh firmware only long enough to test a direct two-board link.

- PELVIS / J-Link `760224787`: direct UWB initiator
- HEAD / J-Link `760225805`: direct UWB responder
- PELVIS diagnostic image SHA-256:
  `88C1348D3401B9EEF22557DA2C2F4F6D43824094207E4F4CB03E91B876D04786`
- HEAD diagnostic image SHA-256:
  `25DE9BBED5805BB3F11C7F900A97CD23B4F1B93DCBD21239B6C1E8997A501EFC`

The mesh images remain available under
`hardware/outputs/dwm3001cdk_motion_node/2026-09-27_explicit_command_mesh_spi_cs_fix/`.

At 40 cm, the direct diagnostic produced 200 valid samples with median 39 cm,
mean 39.02 cm, standard deviation 2.80 cm, and median error -1 cm. This
confirms the two DW3110 radios and their physical placement are working.

The mesh images are restored after this diagnostic.
