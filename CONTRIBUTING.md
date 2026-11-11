# Contributing to DMC1 PC Port

First off, thank you for considering contributing to DMC1 PC Port! 🎮

## 📋 Table of Contents

- [Code of Conduct](#-code-of-conduct)
- [How Can I Contribute?](#-how-can-i-contribute)
- [Development Setup](#-development-setup)
- [Coding Standards](#-coding-standards)
- [Pull Request Process](#-pull-request-process)
- [License](#-license)

## 🛡️ Code of Conduct

This project and everyone participating in it is governed by our Code of Conduct. By participating, you are expected to uphold this code. Please report unacceptable behavior.

## 🤝 How Can I Contribute?

### Reporting Bugs

Before creating bug reports, please check the issue list. When you are creating a bug report, please include as many details as possible:

- Use a clear and descriptive title
- Describe the exact steps to reproduce the bug
- Describe the behavior you observed
- Describe the behavior you expected
- Include screenshots if applicable
- Mention your Windows version and GPU

### Suggesting Enhancements

Enhancement suggestions are tracked as GitHub issues. When creating an enhancement suggestion:

- Use a clear and descriptive title
- Provide a detailed description of the suggested enhancement
- Explain why this enhancement would be useful
- List any similar existing features or alternatives

### Code Contributions

1. Fork the repo and create your branch from `main`
2. Make your changes
3. Test your changes thoroughly
4. Submit a pull request

## 💻 Development Setup

### Prerequisites

- **MinGW-w64** (GCC 11+ for x86_64)
- **MSYS2** (recommended for installing MinGW-w64)
- **OpenGL** (built into Windows 10/11)
- **OpenAL** (optional, for audio)

### Setup Steps

```bash
# Install MinGW-w64 via MSYS2
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make

# Clone the repository
git clone https://github.com/YOUR-USERNAME/dmc1-pc-port.git
cd dmc1-pc-port

# Build the project
make

# Run
make run
```

## 📝 Coding Standards

### General Principles

- **C11 standard** — Use modern C features where appropriate
- **Consistent naming** — Follow existing naming conventions
- **Comments** — Comment complex logic, but don't over-comment
- **Error handling** — Always check for NULL pointers and errors
- **Memory management** — Free all allocated memory

### Naming Conventions

```c
// Files: snake_case
src/game/player.c
include/game/player.h

// Types: CamelCase with Type suffix
typedef struct { ... } PlayerData;
typedef enum { ... } PlayerStateType;

// Functions: Type_action (snake_case)
PlayerData* Player_Create(void);
void Player_Update(PlayerData* player, void* input, double dt);

// Constants: UPPER_CASE
#define MAX_ENTITIES 256
#define PLAYER_SPEED 3.0f

// Variables: snake_case
float player_health;
int entity_count;
```

### File Structure

Every source file should follow this structure:

```c
/*
 * DMC1 PC Port - [Module Name]
 * [Brief description]
 */

#include "[module].h"
#include <other headers>

/* Constants */
#define MAX_SOMETHING 100

/* Internal types */
typedef struct { ... } InternalType;

/* Internal functions */
static void internal_helper(void);

/* ============================================
 * Public API Implementation
 * ============================================ */

ReturnType PublicFunction(Params) {
    // Implementation
}
```

### Git Commit Messages

Follow conventional commit format:

```
type: short description

- Optional detailed description
- Bullet points for multiple changes
```

**Types:**
- `feat:` — New feature
- `fix:` — Bug fix
- `docs:` — Documentation changes
- `style:` — Code style (formatting, semicolons, etc.)
- `refactor:` — Code refactoring
- `test:` — Adding or modifying tests
- `chore:` — Build process or auxiliary tool changes

**Examples:**

```
feat: add player dodge system

- Implement dodge animation states
- Add invincibility frames during dodge
- Fix dodge direction relative to camera
```

## 🔀 Pull Request Process

1. **Fork** the repository
2. **Create a branch** for your feature or bugfix:
   ```bash
   git checkout -b feat/new-combo-system
   ```
3. **Make your changes** following the coding standards
4. **Test thoroughly** — ensure the game compiles and runs
5. **Commit** with clear, descriptive commit messages
6. **Push** to your fork
7. **Open a Pull Request** with:
   - Clear title
   - Detailed description of changes
   - Reference to related issues (if any)
   - Screenshots or videos if visual changes

### PR Checklist

- [ ] Code compiles without warnings
- [ ] Changes tested on Windows x86_64
- [ ] Follows coding standards
- [ ] Includes appropriate comments
- [ ] No memory leaks (if applicable)
- [ ] License headers present in new files
- [ ] README updated if needed

## 📜 License

By contributing, you agree that your contributions will be licensed under the project's custom license. See [LICENSE](LICENSE) for details.

**Important:** All contributions must comply with the license terms:
- Personal/educational use only
- No commercial distribution
- No distribution on game stores (Steam, etc.)
- Proper attribution required

---

<div align="center">

**Thank you for contributing to DMC1 PC Port!** 🙏

*Remember: This project is for educational purposes only. If you want to play DMC1 officially, please purchase it.*

</div>
