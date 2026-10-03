{ lib
, stdenv
, cmake
, ninja
, pkg-config
, qt6
, zlib
, giflib
, vulkan-loader
, vulkan-headers
}:

stdenv.mkDerivation {
  pname = "thulium";
  version = "0.1.0";

  src = lib.cleanSourceWith {
    src = ./.;
    filter = path: type:
      let
        base = baseNameOf path;
      in
      !(base == "build" || base == "CMakeFiles" || base == ".flatpak-builder" || base == "build-flatpak");
  };

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    qt6.wrapQtAppsHook
  ];

  buildInputs = [
    qt6.qtbase
    qt6.qtsvg
    qt6.qtwayland
    zlib
    giflib
    vulkan-loader
    vulkan-headers
  ];

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DBUILD_TESTING=OFF"
  ];

  meta = {
    description = "Fast, lightweight, and cross-platform raster image editor";
    homepage = "https://github.com/finn-freitag/Thulium";
    license = lib.licenses.mit;
    mainProgram = "thulium";
    platforms = lib.platforms.linux;
  };
}
