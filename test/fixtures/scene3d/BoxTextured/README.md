# BoxTextured fixture

`test/fixtures/scene3d/BoxTextured/BoxTextured.glb` is copied from Khronos
glTF Sample Assets:

https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/BoxTextured

Source file:

https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/BoxTextured/glTF-Binary/BoxTextured.glb

SHA-256:

`b510eca2e2ef33f62f9ed57d6e7ce2d10ebb2bdebc4a8e59d347719ba81abdf4`

License and attribution:

- Box Textured model assets: Copyright 2017, Cesium.
- Licensed as CC-BY 4.0 International with Trademark Limitations
  (`LicenseRef-CC-BY-TM`) in the upstream model `LICENSE.md`.
- Cesium logo mark: Copyright 2015, Cesium. Cesium Trademark or Logo
  (`LicenseRef-LegalMark-Cesium`) in the upstream metadata.

This fixture is used only by native Scene3D tests to validate that the
official textured-box GLB can parse through fastgltf into `SceneData` and render
through Pulp's no-JS `Renderer3D` path.
