# Security Policy

## 🛡️ Reporting a Vulnerability

We take the security of DMC1 PC Port seriously. If you discover a security vulnerability, please report it responsibly.

### How to Report

**Do NOT create a public GitHub issue for security vulnerabilities.**

Instead, contact the maintainer directly:

- **GitHub:** Open a private conversation with [@kzdan](https://github.com/kzdan)
- **Email:** [Если есть email для безопасности]

### What to Include

When reporting a security vulnerability, please include:

1. A clear description of the vulnerability
2. Steps to reproduce the issue
3. Potential impact assessment
4. Suggested fix (if you have one)

### Response Timeline

- **Initial response:** Within 48 hours
- **Status update:** Within 1 week
- **Resolution target:** Within 30 days (depending on severity)

## 📋 Supported Versions

| Version | Supported |
|---------|-----------|
| Latest (main branch) | ✅ Yes |
| Previous releases | ❌ No |

> **Note:** DMC1 PC Port is in active development. Only the latest version receives security updates.

## 🔒 Security Measures

### Current Security Practices

- ✅ No network code (offline-only application)
- ✅ No user input parsing from external sources
- ✅ No third-party dependencies with security concerns
- ✅ Memory-safe patterns (bounds checking where applicable)

### Planned Security Improvements

- [ ] Memory sanitization in build (`-fsanitize=address`)
- [ ] Code signing for release builds
- [ ] Automated security scanning in CI/CD

## ⚠️ Known Limitations

- The project does NOT implement input validation for file paths (relies on user providing valid data)
- No sandboxing or isolation (runs as standard Windows application)

## 📜 Disclaimer

This project is a reverse engineering effort for educational purposes. The maintainer is not responsible for any security issues arising from:

- Use of modified or unofficial PS2 data files
- Compilation from untrusted sources
- Use on systems with existing security vulnerabilities

## 🙏 Thank You

Thank you for helping keep DMC1 PC Port secure! Your responsible disclosure helps improve the project for everyone.

---

*Devil May Cry © Capcom Co., Ltd. This project is not affiliated with or endorsed by Capcom.*
