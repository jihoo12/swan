{
  description = "Swan — a small Vulkan 3D engine and procedural garden";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      eachSystem = f: nixpkgs.lib.genAttrs systems (system: f (import nixpkgs { inherit system; }));
      build = pkgs: pkgs.stdenv.mkDerivation {
        pname = "swan";
        version = "0.7.0";
        src = pkgs.lib.cleanSourceWith {
          src = self;
          filter = path: type:
            let name = baseNameOf path;
            in !(builtins.elem name [ "build" "result" ".agents" ".codex" ".aws" ])
              && pkgs.lib.cleanSourceFilter path type;
        };
        nativeBuildInputs = [ pkgs.cmake pkgs.ninja pkgs.pkg-config pkgs.glslang ];
        buildInputs = [ pkgs.vulkan-loader pkgs.vulkan-headers pkgs.glfw pkgs.glm pkgs.nlohmann_json pkgs.tinyobjloader pkgs.libpng ];
        cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release" ];
        doCheck = true;
        meta.mainProgram = "swan";
      };
    in {
      packages = eachSystem (pkgs: { default = build pkgs; });
      checks = eachSystem (pkgs: { build = build pkgs; });
      devShells = eachSystem (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ (build pkgs) ];
          packages = [ pkgs.gdb pkgs.vulkan-tools pkgs.vulkan-validation-layers pkgs.xorg-server pkgs.mesa ];
          VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";
          SWAN_SOFTWARE_ICD = "${pkgs.mesa}/share/vulkan/icd.d/lvp_icd.${if pkgs.stdenv.hostPlatform.isAarch64 then "aarch64" else "x86_64"}.json";
          shellHook = ''
            echo "Swan | cmake -S . -B build -G Ninja && cmake --build build"
          '';
        };
      });
    };
}
