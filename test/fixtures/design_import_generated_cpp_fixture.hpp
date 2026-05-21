#pragma once

#include <memory>
#include <pulp/view/design_import.hpp>
#include <pulp/view/view.hpp>

namespace pulp::test::design_import_cpp_fixture {

std::unique_ptr<pulp::view::View> build_imported_ui();
pulp::view::IRAssetManifest bake_asset_manifest();

}  // namespace pulp::test::design_import_cpp_fixture
