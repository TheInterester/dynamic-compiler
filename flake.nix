{
  description = "Development environment with git, build tools, gdb, qemu, and RISC-V cross-compilation toolchain";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        devShells.default = pkgs.mkShell {
	buildInputs = with pkgs; [
	git
	delta
	cmake
	python3
	gcc
	openssl
	pkg-config
	];
        };
      });
}

