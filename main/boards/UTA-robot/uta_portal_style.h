#pragma once

namespace uta {
inline constexpr char kPortalStyle[] = R"STYLE(<style>
:root{--bg:#e8edf0;--sf:#fff;--tx:#10212b;--mu:#66767f;--ln:#d3dbe0;--ac:#10212b;--at:#fff;--bad:#d62839;--c1:#3b82f6;--c2:#f59e0b;--c3:#8b5cf6;--c4:#10b981;--c5:#f43f5e;--c6:#14b8a6;--c7:#64748b;box-sizing:border-box;padding-top:env(safe-area-inset-top,0px);padding-bottom:env(safe-area-inset-bottom,0px)}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--bg:#0d161b;--sf:#15222a;--tx:#e6eef2;--mu:#8a9ba5;--ln:#26373f;--ac:#e6eef2;--at:#0d161b}}
:root[data-theme="dark"]{--bg:#0d161b;--sf:#15222a;--tx:#e6eef2;--mu:#8a9ba5;--ln:#26373f;--ac:#e6eef2;--at:#0d161b}

body{margin:0;background:var(--bg);color:var(--tx);font:14px/1.45 'Bricolage Grotesque',system-ui,sans-serif}
body .page{width:min(100%,640px);max-width:none;margin:0 auto;padding:20px 16px 40px}
body .app-header{color:var(--tx);background:none;text-shadow:none}
body .app-header .logo{background:var(--sf);color:var(--c1);box-shadow:none;border:1px solid var(--ln)}
body .card,body .section,body .loading-content,body .toast{color:var(--tx);background:var(--sf);border:1px solid var(--ln);border-radius:12px;box-shadow:none}
body .section{border-left:4px solid var(--c1);padding:14px;margin:12px 0}
body .tabs{background:none;border-color:var(--ln);gap:6px;padding:4px}
body .tab{background:var(--sf);border:1px solid var(--ln);border-radius:999px;color:var(--tx)}
body .tab.active{background:var(--ac);border-color:var(--ac);color:var(--at)}
body h1,body h2,body h3,body label,body .loading-text{color:var(--tx)}
body input:not([type=checkbox]),body select{border:1px solid var(--ln);border-radius:8px;background:var(--bg);color:var(--tx);box-shadow:none}
body #ota_url,body #uta-province{box-sizing:border-box;width:100%;min-width:0;height:44px;padding:0 12px;font:inherit}
body #ota_url_row .icon-btn{flex:none;width:44px;height:44px}
body button,body input[type=submit]{background:var(--ac);color:var(--at);border:1px solid var(--ac);border-radius:8px;box-shadow:none;transform:none}
body button:hover,body input[type=submit]:hover{background:var(--ac);color:var(--at);box-shadow:none;transform:none}
body .ap-item,body .saved-item,body .switch-row,body #uta-location-panel{background:var(--sf);color:var(--tx);border-color:var(--ln);box-shadow:none}
body .error{color:var(--bad)}body .scanning,body small{color:var(--mu)}
body :focus-visible{outline:2px solid var(--c1);outline-offset:2px}
body .card,body .section,body button,body .tab{animation:none;transition:none}
</style>)STYLE";
}  // namespace uta
