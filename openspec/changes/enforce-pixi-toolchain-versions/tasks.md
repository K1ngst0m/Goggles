## 1. Pin Build Tools

- [x] 1.1 Pin cmake = "3.31.*"
- [x] 1.2 Pin ninja = "1.12.*"
- [x] 1.3 Pin clang_linux-64 = "21.*"
- [x] 1.4 Pin clangxx_linux-64 = "21.*"
- [x] 1.5 Pin lld = "21.*"
- [x] 1.6 Pin ccache = "4.*"

## 2. Pin Dev Tools

- [x] 2.1 Pin taplo = "==0.9.3" in default env (match lint)

## 3. Pin Libraries

- [x] 3.1 Pin sdl3 = "3.2.*"

## 4. Verification

- [x] 4.1 Run `pixi install` to verify resolution
- [x] 4.2 Run `pixi run build debug` to verify build
- [x] 4.3 Run `pixi run test debug` to verify tests (100% passed)
- [x] 4.4 Verify clang-tidy uses pinned version in quality build
