#!/usr/bin/env python3
import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def final_manifest():
    return {
        "schema_version": 1,
        "golden_kind": "renderer3d_rgba_fingerprint",
        "fingerprint_algorithm": "fnv1a64",
        "status": "final_software_adapter",
        "entries": [
            {
                "id": "hardcoded_textured_cube",
                "renderer": "Renderer3D::render_hardcoded_textured_cube",
                "source": "hardcoded",
                "width": 128,
                "height": 128,
                "adapter_backend_type": "SwiftShader",
                "adapter_scope": "software_swiftshader",
                "scene_data_consumed": False,
                "primitive_count": 0,
                "depth_target_allocated": True,
                "color_target_allocated": True,
                "vertex_buffer_uploaded": True,
                "index_buffer_uploaded": True,
                "uniform_buffer_uploaded": True,
                "texture_uploaded": True,
                "pipeline_cache_entry_count": 0,
                "pipeline_cache_hit_count": 0,
                "command_submitted": True,
                "readback_completed": True,
                "pixel_output_produced": True,
                "min_distinct_color_count": 64,
                "min_non_transparent_pixel_count": 1024,
                "fingerprint": "1111111111111111111",
                "cpp_constant": "kFutureSoftwareHardcodedCubeFingerprint",
            }
        ],
        "software_adapter": {
            "status": "pinned",
            "required_for_final_phase7": True,
            "pixel_producing": True,
            "backend_type": "SwiftShader",
            "adapter_scope": "software_swiftshader",
            "golden_entry_ids": ["hardcoded_textured_cube"],
            "notes": "Synthetic final manifest used to verify probe-contract behavior.",
        },
    }


def fake_probe_source(final_eligible):
    final_value = "true" if final_eligible else "false"
    lines = [
        "scene=hardcoded",
        "width=128",
        "height=128",
        "success=true",
        "gpu_available=true",
        "adapter_info_available=true",
        "adapter_backend_type=SwiftShader",
        "adapter_backend=software",
        "adapter_name=SwiftShader",
        "adapter_vendor=Google",
        "adapter_architecture=cpu",
        "adapter_scope=software_swiftshader",
        "adapter_backend_preference=default",
        "fallback_adapter_requested=false",
        "null_backend_requested=false",
        "scene_data_consumed=false",
        "depth_target_allocated=true",
        "color_target_allocated=true",
        "vertex_buffer_uploaded=true",
        "index_buffer_uploaded=true",
        "uniform_buffer_uploaded=true",
        "texture_uploaded=true",
        "texture_decoded=false",
        "fallback_texture_used=false",
        "base_color_texture_srgb_applied=true",
        "texture_sampler_applied=true",
        "texture_sampler_clamp_s=false",
        "texture_sampler_clamp_t=false",
        "texture_sampler_linear=true",
        "texture_mipmap_filter_downgraded=false",
        "base_color_transform_applied=false",
        "base_color_texcoord1_used=false",
        "base_color_factor_applied=true",
        "unlit_material_applied=false",
        "alpha_mask_applied=false",
        "alpha_blend_applied=false",
        "alpha_blend_depth_write_disabled=false",
        "alpha_blend_sorted=false",
        "vertex_color_applied=false",
        "metallic_roughness_factor_applied=false",
        "metallic_roughness_texture_applied=false",
        "double_sided_material_applied=false",
        "emissive_factor_applied=false",
        "emissive_strength_applied=false",
        "emissive_texture_applied=false",
        "geometry_normals_applied=false",
        "directional_light_applied=false",
        "directional_light_transform_applied=false",
        "light_node_transform_deferred=false",
        "point_light_applied=false",
        "point_light_deferred=false",
        "spot_light_applied=false",
        "spot_light_deferred=false",
        "punctual_light_range_applied=false",
        "punctual_light_range_deferred=false",
        "spot_light_cone_deferred=false",
        "perspective_camera_applied=false",
        "orthographic_camera_applied=false",
        "camera_node_translation_applied=false",
        "camera_node_rotation_applied=false",
        "camera_aspect_ratio_applied=false",
        "camera_aspect_ratio_deferred=false",
        "camera_depth_range_applied=false",
        "camera_depth_range_deferred=false",
        "camera_node_transform_deferred=false",
        "transform_animation_initial_pose_applied=false",
        "transform_animation_deferred=false",
        "tangent_attributes_available=false",
        "tangent_attributes_derived=false",
        "normal_texture_applied=false",
        "normal_scale_applied=false",
        "metallic_roughness_texture_deferred=false",
        "normal_texture_deferred=false",
        "normal_scale_deferred=false",
        "occlusion_texture_applied=false",
        "occlusion_strength_applied=false",
        "occlusion_texture_deferred=false",
        "occlusion_strength_deferred=false",
        "emissive_texture_deferred=false",
        "non_base_color_texture_transform_applied=false",
        "non_base_color_texcoord1_used=false",
        "non_base_color_texture_transform_deferred=false",
        "non_base_color_texcoord1_deferred=false",
        "advanced_material_extension_deferred=false",
        "skinning_deferred=false",
        "morph_target_deferred=false",
        "gpu_instancing_deferred=false",
        "command_submitted=true",
        "readback_completed=true",
        "primitive_count=0",
        "pipeline_cache_entry_count=0",
        "pipeline_cache_hit_count=0",
        "distinct_color_count=64",
        "non_transparent_pixel_count=1024",
        "rgba_fingerprint=1111111111111111111",
        "pixel_output_produced=true",
        f"final_software_golden_eligible={final_value}",
    ]
    return "print(" + repr("\n".join(lines) + "\n") + ", end='')\n"


def run_verifier(verifier, manifest, probe):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--manifest",
            str(manifest),
            "--entry-id",
            "hardcoded_textured_cube",
            "--",
            sys.executable,
            str(probe),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def main():
    parser = argparse.ArgumentParser(
        description="Verify renderer probe final-software eligibility contract.")
    parser.add_argument("--probe-verifier", type=Path, required=True)
    args = parser.parse_args()

    if not args.probe_verifier.exists():
        print(f"probe_verifier_exists=false path={args.probe_verifier}")
        return 2

    errors = []
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        manifest_path = temp_path / "final-software-goldens.json"
        manifest_path.write_text(
            json.dumps(final_manifest()),
            encoding="utf-8")

        good_probe = temp_path / "probe-final-true.py"
        good_probe.write_text(fake_probe_source(True), encoding="utf-8")
        result = run_verifier(args.probe_verifier, manifest_path, good_probe)
        sys.stdout.write(result.stdout)
        if result.returncode != 0:
            errors.append(
                f"final-eligible probe expected exit 0, got {result.returncode}")
        if "renderer_probe_verified=hardcoded_textured_cube" not in result.stdout:
            errors.append("final-eligible probe did not verify manifest entry")
        print("renderer_probe_final_eligibility_verified=true")

        bad_probe = temp_path / "probe-final-false.py"
        bad_probe.write_text(fake_probe_source(False), encoding="utf-8")
        result = run_verifier(args.probe_verifier, manifest_path, bad_probe)
        sys.stdout.write(result.stdout)
        if result.returncode != 1:
            errors.append(
                f"final-ineligible probe expected exit 1, got {result.returncode}")
        if "final_software_golden_eligible: expected 'true', got 'false'" not in result.stdout:
            errors.append(
                "final-ineligible probe did not report the expected eligibility mismatch")
        print("renderer_probe_final_eligibility_rejected=false_probe")

    if errors:
        for error in errors:
            print(f"error={error}")
        return 1
    print("renderer_probe_final_eligibility_contract_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
