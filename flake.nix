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
      inherit (nixpkgs) lib;

      pkgsFor = system:
        import nixpkgs {
          inherit system;
          overlays = [ ];
        };

      targetSystems = [ "aarch64-linux" "x86_64-linux" ];
    in {
      overlays.default = final: prev: {
        swayfx = prev.sway-unwrapped.overrideAttrs
          (old: { src = builtins.path { path = prev.lib.cleanSource ./.; }; });
      };

      packages = nixpkgs.lib.genAttrs targetSystems (system:
        let pkgs = pkgsFor system;
        in (self.overlays.default pkgs pkgs) // {
          default = self.packages.${system}.swayfx;
        });

      devShells = nixpkgs.lib.genAttrs targetSystems (system:
        let pkgs = pkgsFor system;
        in {
          default = pkgs.mkShell {
            depsBuildBuild = with pkgs; [ pkg-config ];

            nativeBuildInputs = with pkgs; [
              cmake
              meson
              ninja
              pkg-config
              wayland-scanner
              scdoc
            ];

            buildInputs = with pkgs; [
              wayland
              libxkbcommon
              pcre
              json_c
              libevdev
              pango
              cairo
              libinput
              libcap
              pam
              gdk-pixbuf
              librsvg
              wayland-protocols
              libdrm
              wlroots
              dbus
              xwayland
              libGL
              pixman
              xorg.xcbutilwm
              xorg.libX11
              libcap
              xorg.xcbutilimage
              xorg.xcbutilerrors
              mesa
              libpng
              ffmpeg
              xorg.xcbutilrenderutil
              seatd
            ];
          };
        });
    };
}
