#!/usr/bin/env python3
"""
scripts/generate_release_notes.py
Automated Git Commit Parser, Changelog, Patch Notes & Release Metadata Generator for NeoNect.
"""

import subprocess
import json
import os
import re
import sys
from datetime import datetime, timezone

def run_cmd(cmd, cwd=None):
    try:
        res = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=True)
        return res.stdout.strip()
    except Exception:
        return ""

def get_git_info(repo_root):
    tag = run_cmd(["git", "describe", "--tags", "--match", "v*", "--abbrev=0"], cwd=repo_root)
    if not tag or not re.match(r"^v?\d+\.\d+\.\d+", tag):
        tag = os.environ.get("NEONECT_VERSION", "v1.0.0")
    if not tag.startswith("v"):
        tag = "v" + tag

    commit_hash = run_cmd(["git", "rev-parse", "--short", "HEAD"], cwd=repo_root) or "latest"
    branch = run_cmd(["git", "rev-parse", "--abbrev-ref", "HEAD"], cwd=repo_root) or "main"
    
    return tag, commit_hash, branch

def get_commits_since_last_tag(repo_root):
    tags = run_cmd(["git", "tag", "--sort=-creatordate"], cwd=repo_root).splitlines()
    rev_range = "HEAD"
    if len(tags) >= 2:
        rev_range = f"{tags[1]}..HEAD"
    elif len(tags) == 1:
        rev_range = f"{tags[0]}..HEAD"

    raw_log = run_cmd(["git", "log", rev_range, "--pretty=format:%h|||%s|||%b|||%an|||%ad", "--date=short"], cwd=repo_root)
    if not raw_log:
        raw_log = run_cmd(["git", "log", "-n", "30", "--pretty=format:%h|||%s|||%b|||%an|||%ad", "--date=short"], cwd=repo_root)

    commits = []
    for line in raw_log.splitlines():
        parts = line.split("|||")
        if len(parts) >= 2 and parts[0] and len(parts[0]) >= 4:
            sub = parts[1].strip()
            if not sub or sub.startswith("Merge ") or len(sub) < 3:
                continue
            commits.append({
                "hash": parts[0].strip(),
                "subject": sub,
                "body": parts[2].strip() if len(parts) > 2 else "",
                "author": parts[3].strip() if len(parts) > 3 else "",
                "date": parts[4].strip() if len(parts) > 4 else ""
            })
    return commits

def categorize_commits(commits):
    categories = {
        "security": [],
        "features": [],
        "performance": [],
        "fixes": [],
        "improvements": []
    }

    for c in commits:
        sub = c["subject"]
        lower = sub.lower()
        
        # Clean conventional commit prefix
        clean_text = re.sub(r"^(feat|fix|perf|sec|security|refactor|chore|docs|style|test|ci)(\([^\)]+\))?:\s*", "", sub, flags=re.IGNORECASE).strip()
        if not clean_text or clean_text == c["author"] or len(clean_text) < 3:
            continue
        entry = f"{clean_text} (`{c['hash']}`)"

        if any(k in lower for k in ["sec:", "security", "encrypt", "crypto", "e2ee", "e2e", "aes-256", "pbkdf2"]):
            categories["security"].append(entry)
        elif lower.startswith("feat") or "feature" in lower:
            categories["features"].append(entry)
        elif lower.startswith("perf") or any(k in lower for k in ["memory", "lazy", "optimize", "speed", "cache"]):
            categories["performance"].append(entry)
        elif lower.startswith("fix") or "bug" in lower or "resolve" in lower or "crash" in lower:
            categories["fixes"].append(entry)
        else:
            categories["improvements"].append(entry)

    return categories

def build_release_notes_md(tag, commit_hash, categories, commits):
    today = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    md = []
    md.append(f"# NeoNect {tag} Release Notes & Changelog\n")
    md.append(f"**Release Date**: {today}  \n**Commit**: `{commit_hash}`  \n**Supported Platforms**: Windows x86_64, Linux x86_64  \n**Documentation**: [https://neonect-devs.github.io/NeoNect-desktop/](https://neonect-devs.github.io/NeoNect-desktop/)\n")
    
    md.append("## 🌟 Release Summary & Highlights\n")
    md.append(f"NeoNect `{tag}` delivers production-ready decentralized, end-to-end encrypted (E2EE) real-time messaging with high-performance desktop client streaming.\n")

    if categories["security"]:
        md.append("### 🔒 Security & Cryptography (E2EE)")
        for item in categories["security"]:
            md.append(f"- {item}")
        md.append("")

    if categories["features"]:
        md.append("### 🚀 New Features & Capabilities")
        for item in categories["features"]:
            md.append(f"- {item}")
        md.append("")

    if categories["performance"]:
        md.append("### ⚡ Performance & Memory Optimizations")
        for item in categories["performance"]:
            md.append(f"- {item}")
        md.append("")

    if categories["fixes"]:
        md.append("### 🐛 Bug Fixes & Stability")
        for item in categories["fixes"]:
            md.append(f"- {item}")
        md.append("")

    if categories["improvements"]:
        md.append("### 🛠️ General Improvements & Toolchain")
        for item in categories["improvements"]:
            md.append(f"- {item}")
        md.append("")

    # Full commit history section for the release
    if commits:
        md.append("### 📜 Detailed Commit Changelog")
        for c in commits:
            md.append(f"- `{c['hash']}` {c['subject']} - *{c['author']}*")
        md.append("")

    md.append("---")
    md.append("### 📦 Asset Checksums & Verification")
    md.append("- All Windows `.zip` and Linux `.tar.gz` distribution packages contain full standalone runtimes and OpenSSL cryptographic libraries.")
    md.append("- For comprehensive API references and architecture diagrams, see the [Online Documentation](https://neonect-devs.github.io/NeoNect-desktop/).\n")

    return "\n".join(md)

def update_changelog_file(file_path, is_doxygen, tag, categories):
    today = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    
    existing_content = ""
    if os.path.exists(file_path):
        with open(file_path, "r", encoding="utf-8") as f:
            existing_content = f.read()

    # Generate the version block
    version_lines = []
    version_lines.append(f"## [{tag}] - {today}\n")
    version_lines.append(f"### 🚀 Production Release {tag}\n")

    if categories["security"]:
        version_lines.append("#### Security & Cryptography")
        for item in categories["security"]:
            version_lines.append(f"- {item}")
        version_lines.append("")

    if categories["features"]:
        version_lines.append("#### Added")
        for item in categories["features"]:
            version_lines.append(f"- {item}")
        version_lines.append("")

    if categories["improvements"]:
        version_lines.append("#### Changed")
        for item in categories["improvements"]:
            version_lines.append(f"- {item}")
        version_lines.append("")

    if categories["fixes"]:
        version_lines.append("#### Fixed")
        for item in categories["fixes"]:
            version_lines.append(f"- {item}")
        version_lines.append("")

    if categories["performance"]:
        version_lines.append("#### Performance")
        for item in categories["performance"]:
            version_lines.append(f"- {item}")
        version_lines.append("")

    version_block = "\n".join(version_lines) + "\n---\n"

    # Header definition
    if is_doxygen:
        header = "# Changelog & Release History {#changelog}\n\nAll notable changes to the **NeoNect Desktop Client** will be documented in this file.\n\nThe format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),\nand this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).\n\n---\n\n## [Unreleased]\n\n### Planned\n- Group chat support with MLS (Messaging Layer Security) E2EE tree protocol.\n- WebRTC peer-to-peer live voice and video calling.\n- Hardware key token (YubiKey / PKCS#11) hardware vault integration.\n\n---\n"
    else:
        header = "# Changelog & Release Notes\n\nAll notable changes to the **NeoNect Desktop Client** will be documented in this file.\n\nThe format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),\nand this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).\n\nFor the interactive documentation site version, see [docs/CHANGELOG.md](docs/CHANGELOG.md) or visit the [Online Documentation](https://neonect-devs.github.io/NeoNect-desktop/changelog.html).\n\n---\n\n## [Unreleased]\n\n### Planned\n- Group chat support with MLS (Messaging Layer Security) E2EE tree protocol.\n- WebRTC peer-to-peer live voice and video calling.\n- Hardware key token (YubiKey / PKCS#11) hardware vault integration.\n\n---\n"

    # If tag already present, don't duplicate; replace its section or prepend
    tag_header_pattern = rf"## \[{re.escape(tag)}\]"
    if re.search(tag_header_pattern, existing_content):
        # Already has section for this tag, preserve existing or update
        updated_content = existing_content
    else:
        # Insert right after [Unreleased] section
        unreleased_match = re.search(r"## \[Unreleased\].*?---\n", existing_content, re.DOTALL)
        if unreleased_match:
            insert_pos = unreleased_match.end()
            updated_content = existing_content[:insert_pos] + "\n" + version_block + "\n" + existing_content[insert_pos:]
        else:
            updated_content = header + "\n" + version_block + "\n" + existing_content

    with open(file_path, "w", encoding="utf-8") as f:
        f.write(updated_content.strip() + "\n")
    print(f"[+] Updated Changelog at: {file_path}")

def generate_patch_notes_page(docs_patch_notes_path, tag, commit_hash, categories):
    today = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    md = []
    md.append("# Patch Notes & Highlights {#patch_notes}\n")
    md.append("This document outlines the major patch highlights, security hardening updates, and architectural enhancements for all production releases of **NeoNect Desktop**.\n")
    md.append("---\n")
    md.append(f"## Version {tag} ({today})\n")
    md.append(f"| Property | Specification |\n| :--- | :--- |\n| **Version** | `{tag}` |\n| **Release Date** | {today} |\n| **Commit Hash** | `{commit_hash}` |\n| **Platform Targets** | Windows x86_64 (MSVC 2022), Linux x86_64 (GCC 11+) |\n| **Qt Framework** | Qt 6.5+ LTS / 6.8+ |\n| **Cryptography Engine** | OpenSSL 3.x / 4.x (AES-256-GCM + PBKDF2) |\n")
    
    md.append("### Key Release Highlights\n")
    
    all_items = categories["security"] + categories["features"] + categories["performance"] + categories["fixes"] + categories["improvements"]
    if all_items:
        for item in all_items[:12]:
            md.append(f"* ✔️ {item}")
    else:
        md.append("* ✔️ Enterprise-grade End-to-End Encryption (E2EE) with zero-server-knowledge relay.")
        md.append("* ✔️ Responsive QtQuick Controls presentation layer with custom dark/light themes.")
        md.append("* ✔️ Automated multi-platform Windows and Linux CI/CD release deployment.")

    md.append("\n### Security Advisories & Hardening\n")
    md.append("- **Cryptographic Nonces**: Guaranteed 96-bit CSPRNG unique nonces for every single `AES-256-GCM` payload.")
    md.append("- **Memory Safety**: Strict RAII encapsulation for all OpenSSL EVP contexts.")
    md.append("- **Local Storage**: Hardware-derived machine-key encrypted SQLite credential vault.\n")

    md.append("---\n")
    md.append("### Historical Patch Notes Archive\n")
    md.append("* 📜 For full commit-level tracking, refer to the **@subpage changelog \"Comprehensive Changelog\"**.\n")

    with open(docs_patch_notes_path, "w", encoding="utf-8") as f:
        f.write("\n".join(md))
    print(f"[+] Generated Documentation Patch Notes at: {docs_patch_notes_path}")

def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    tag, commit_hash, branch = get_git_info(repo_root)
    commits = get_commits_since_last_tag(repo_root)
    categories = categorize_commits(commits)

    # 1. Output RELEASE_NOTES.md (Used as body for GitHub Draft Release)
    rel_notes_md = build_release_notes_md(tag, commit_hash, categories, commits)
    rel_notes_path = os.path.join(repo_root, "RELEASE_NOTES.md")
    with open(rel_notes_path, "w", encoding="utf-8") as f:
        f.write(rel_notes_md)
    print(f"[+] Generated Release Notes at: {rel_notes_path}")

    # 2. Update Root CHANGELOG.md & docs/CHANGELOG.md
    update_changelog_file(os.path.join(repo_root, "CHANGELOG.md"), is_doxygen=False, tag=tag, categories=categories)
    update_changelog_file(os.path.join(repo_root, "docs", "CHANGELOG.md"), is_doxygen=True, tag=tag, categories=categories)

    # 3. Generate docs/PATCH_NOTES.md for Doxygen site
    docs_patch_notes_path = os.path.join(repo_root, "docs", "PATCH_NOTES.md")
    generate_patch_notes_page(docs_patch_notes_path, tag, commit_hash, categories)

    # 4. Output assets/patch_notes.json (for in-app QML viewer)
    all_highlights = []
    for cat_name in ["security", "features", "performance", "fixes", "improvements"]:
        for item in categories[cat_name]:
            clean_item = re.sub(r"\s*\(`[a-f0-9]+`\)$", "", item)
            all_highlights.append(clean_item)

    if not all_highlights:
        all_highlights = [
            "Complete End-to-End Encryption for all messages, audio, video, and avatars.",
            "Pervasive UI lazy-loading architecture with ~40% memory reduction.",
            "Hardware-accelerated media lightbox with zoom, seek, and multi-format playback.",
            "Real-time presence and customizable status switching.",
            "Production-ready Windows and Linux desktop packaging."
        ]

    patch_entry = {
        "version": tag.lstrip("v"),
        "tag": tag,
        "date": datetime.now(timezone.utc).strftime("%Y-%m-%d"),
        "commit": commit_hash,
        "title": f"NeoNect {tag} Production Release",
        "highlights": all_highlights[:15]
    }

    patch_json_path = os.path.join(repo_root, "assets", "patch_notes.json")
    os.makedirs(os.path.dirname(patch_json_path), exist_ok=True)
    with open(patch_json_path, "w", encoding="utf-8") as f:
        json.dump([patch_entry], f, indent=2)
    print(f"[+] Generated In-App Patch Notes at: {patch_json_path}")

if __name__ == "__main__":
    main()
