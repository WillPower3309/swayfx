{
  description = "swaywm development environment";

  inputs = {
    flake-compat = {
      url = "github:edolstra/flake-compat";
      flake = false;
    };

    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
  };

  outputs = { self, nixpkgs, flake-compat, ... }:
    let
      pkgsFor = system:
        import nixpkgs {
          inherit system;
          overlays = [ ];
        };

      targetSystems = [ "aarch64-linux" "x86_64-linux" ];
    in
    {
      overlays.default = final: prev: {
        swayfx-unwrapped = prev.sway-unwrapped.overrideAttrs (old: {
          src = builtins.path { path = prev.lib.cleanSource ./.; };
          patches =
            let
              removePatches = [
                "LIBINPUT_CONFIG_ACCEL_PROFILE_CUSTOM.patch"
              ];
            in
            builtins.filter
              (patch: !builtins.elem (patch.name or null) removePatches)
              (old.patches or [ ]);
        });
      };

      packages = nixpkgs.lib.genAttrs targetSystems (system:
        let pkgs = pkgsFor system;
        in (self.overlays.default pkgs pkgs) // {
          default = self.packages.${system}.swayfx-unwrapped;
        });

      devShells = nixpkgs.lib.genAttrs targetSystems (system:
        let
          pkgs = pkgsFor system;
        in
        {
          default = pkgs.mkShell {
            name = "swayfx-shell";
            depsBuildBuild = with pkgs; [ pkg-config ];
            inputsFrom = [ self.packages.${system}.swayfx-unwrapped pkgs.wlroots_0_17 ];

            nativeBuildInputs = with pkgs; [
              cmake
              meson
              ninja
              pkg-config
              wayland-scanner
              scdoc
              hwdata # for wlroots
            ];

            shellHook = with pkgs; ''(
              mkdir -p "$PWD/subprojects"
              cd "$PWD/subprojects"
              cp -R --no-preserve=mode,ownership ${wlroots_0_17.src} wlroots
            )'';
          };
        });
    };
}
