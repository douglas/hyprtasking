{
  description = "Hyprtasking";

  inputs = {
    # A supported release; consumers can override it with inputs.hyprland.follows.
    hyprland.url = "github:hyprwm/Hyprland/v0.56.2";
    nixpkgs.follows = "hyprland/nixpkgs";
    systems.url = "github:nix-systems/default-linux";
  };

  outputs = {
    self,
    nixpkgs,
    hyprland,
    systems,
    ...
  }: let
    inherit (nixpkgs) lib;

    forSystems = attrs:
      lib.genAttrs (import systems) (
        system:
          attrs system nixpkgs.legacyPackages.${system}
      );
  in {
    packages = forSystems (system: pkgs: {
      hyprtasking = let
        hyprlandPkg = hyprland.packages.${system}.hyprland;
      in
        pkgs.stdenv.mkDerivation {
          pname = "hyprtasking";
          version = "0.6.1";

          src = ./.;

          nativeBuildInputs = [pkgs.meson pkgs.ninja] ++ hyprlandPkg.nativeBuildInputs;
          buildInputs = [hyprlandPkg] ++ hyprlandPkg.buildInputs;

          meta = with lib; {
            homepage = "https://github.com/douglas/hyprtasking";
            description = "Powerful workspace management plugin, packed with features ";
            license = licenses.bsd3;
            platforms = platforms.linux;
          };
        };

      default = self.packages.${system}.hyprtasking;
    });
  };
}
