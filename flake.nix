{
  description = "Swan — a small Vulkan 3D engine, procedural garden, and scene editor";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    # Editor UI sources are pinned in flake.lock; nixpkgs ships only the non-docking imgui branch.
    imgui = { url = "github:ocornut/imgui/v1.92.9b-docking"; flake = false; };
    imguizmo = { url = "github:CedricGuillemet/ImGuizmo"; flake = false; };
  };
  outputs = { self, nixpkgs, imgui, imguizmo }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      eachSystem = f: nixpkgs.lib.genAttrs systems (system: f (import nixpkgs { inherit system; }));
      # The same paths configure package builds (cmakeFlags) and dev shells (environment).
      uiEnv = pkgs: {
        SWAN_IMGUI_DIR = "${imgui}";
        SWAN_IMGUIZMO_DIR = "${imguizmo}";
        SWAN_UI_FONT = "${pkgs.inter}/share/fonts/truetype/InterVariable.ttf";
        SWAN_ICON_FONT = "${pkgs.lucide}/share/fonts/truetype/Lucide.ttf";
      };
      build = pkgs: pkgs.stdenv.mkDerivation {
        pname = "swan";
        version = "0.16.0";
        src = pkgs.lib.cleanSourceWith {
          src = self;
          filter = path: type:
            let name = baseNameOf path;
            in !(builtins.elem name [ "build" "result" ".agents" ".codex" ".aws" ])
              && pkgs.lib.cleanSourceFilter path type;
        };
        nativeBuildInputs = [ pkgs.cmake pkgs.ninja pkgs.pkg-config pkgs.glslang ];
        buildInputs = [ pkgs.vulkan-loader pkgs.vulkan-headers pkgs.glfw ];
        # The installed headless SDK (static libraries) needs these in every consumer as well.
        propagatedBuildInputs = [ pkgs.glm pkgs.nlohmann_json pkgs.tinyobjloader pkgs.libpng pkgs.assimp pkgs.lua5_4 ];
        cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release" ]
          ++ pkgs.lib.mapAttrsToList (name: value: "-D${name}=${value}") (uiEnv pkgs);
        doCheck = true;
        meta.mainProgram = "swan";
      };
    in {
      packages = eachSystem (pkgs: { default = build pkgs; });
      checks = eachSystem (pkgs: {
        build = build pkgs;
        # An out-of-tree project built against the installed SDK with find_package(swan).
        headless-example = pkgs.stdenv.mkDerivation {
          name = "swan-headless-example";
          src = ./examples/headless-cpp;
          nativeBuildInputs = [ pkgs.cmake pkgs.ninja pkgs.pkg-config ];
          buildInputs = [ (build pkgs) ];
          doInstallCheck = true;
          installCheckPhase = "$out/bin/garden-stats";
        };
        # Behaviour and automation scripts type-check against lua/types (LuaLS).
        lua-types = pkgs.runCommand "swan-lua-types" { nativeBuildInputs = [ pkgs.lua-language-server ]; } ''
          cp -r ${./lua} lua; cp -r ${./assets/scripts} scripts; cp -r ${./examples/scripts} examples; cp ${./.luarc.json} .luarc.json
          HOME=$TMPDIR lua-language-server --check . --checklevel=Warning --logpath $TMPDIR/log > report.txt 2>&1 || true
          cat report.txt
          grep -q "no problems found" report.txt
          touch $out
        '';
      });
      apps = eachSystem (pkgs: {
        default = { type = "app"; program = "${build pkgs}/bin/swan"; };
        editor = {
          type = "app";
          program = "${pkgs.writeShellScript "swan-editor" ''exec ${build pkgs}/bin/swan editor "$@"''}";
        };
      });
      formatter = eachSystem (pkgs: pkgs.nixpkgs-fmt);
      devShells = eachSystem (pkgs: {
        default = pkgs.mkShell ({
          inputsFrom = [ (build pkgs) ];
          packages = [ pkgs.gdb pkgs.clang-tools pkgs.xdotool pkgs.vulkan-tools pkgs.vulkan-validation-layers pkgs.xorg-server pkgs.mesa pkgs.python3 pkgs.lua-language-server ];
          VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";
          SWAN_SOFTWARE_ICD = "${pkgs.mesa}/share/vulkan/icd.d/lvp_icd.${if pkgs.stdenv.hostPlatform.isAarch64 then "aarch64" else "x86_64"}.json";
          shellHook = ''
            echo "Swan dev shell"
            echo "  cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build"
            echo "  ./build/swan editor assets/scenes/scripted-garden.swan.json"
            echo "  ./build/swan script examples/scripts/scene-report.lua assets/scenes/gltf-garden.swan.json"
          '';
        } // uiEnv pkgs);
      });
    };
}
