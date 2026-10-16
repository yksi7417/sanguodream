// Deny tool calls that reach across the clean-room wall.
// Input: Claude Code PreToolUse JSON on stdin. Output: a deny decision, or nothing.
let raw = '';
process.stdin.on('data', d => raw += d);
process.stdin.on('end', () => {
  let input = {};
  try { input = JSON.parse(raw || '{}'); } catch (e) { input = {}; }
  const ti = input.tool_input || {};
  const hay = [ti.file_path, ti.path, ti.pattern, ti.command, ti.glob, ti.notebook_path]
    .filter(Boolean).join(' ').toLowerCase().replace(/\\/g, '/');
  const forbidden = [
    /(^|\/)raw(\/|$)/,                    // any raw/ folder, incl. spec-src/raw
    /spec-src\/tools/,                    // the analysis side's scripts
    /src\/san11-analysis/,                // the analysis repo itself; use spec-src/spec
    /fixtures-bin/,                       // the game snapshot
    /steamapps\/common\/san11wpk/,        // the real Steam install
    /documents\/koei/,                    // the live save folder
    /san11wpk/, /san11\.exe/,             // the game binaries by name
    /\.gpr(\/|$)/, /\.rep(\/|$)/,         // Ghidra project files
    /ghidra/,                             // projects, exports, the install
    /127\.0\.0\.1:8080|localhost:8080/    // the GhidraMCP HTTP server
  ];
  if (forbidden.some(r => r.test(hay))) {
    process.stdout.write(JSON.stringify({ hookSpecificOutput: {
      hookEventName: 'PreToolUse', permissionDecision: 'deny',
      permissionDecisionReason:
        'Clean-room wall: this path is analysis material. Implement from spec-src/spec only.'
    }}));
  }
  process.exit(0);
});
