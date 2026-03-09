# VRCdlpsetup

VRChatのyt-dlpの問題を根本的に解決するwrapperです

## 機能

- **自動アップデート** — 起動時にyt-dlpの最新バージョンをGitHubからチェックし、更新があれば自動でダウンロード・置き換えを行います
- **Cookie対応** — `cookies.txt` を配置するだけで、yt-dlp実行時に自動的にcookieが適用されます
- **透過的なラッパー** — 引数はすべてそのままyt-dlpに渡されるため、既存の動作に影響を与えません

## How to Build

```bash
cl /std:c++17 /O2 /MT /EHsc VRCdlpsetup.cpp /link winhttp.lib crypt32.lib user32.lib
```

## How to Setup

VRChat内のyt-dlp.exeが置いてあるディレクトリ内にVRCdlpsetup.exeを置いてダブルクリックで起動
```path
C:\Users\***\AppData\LocalLow\VRChat\VRChat\Tools
```

基本的にここに置かれています

自動的に最新版のyt-dlpに置き換わります

## Option

同じディレクトリ内に `cookies.txt` を置いてcookieを貼ることで自動的にcookieを使用するオプションになります

[cookie取得方法](https://wzwi.gitlab.io/entry/2025/05/17/061709/)

## 注意事項

- 使用に関して現在問題は一切発生していませんが完全自己責任で利用してください
- セットアップ時にexe名が `VRCdlpsetup.exe` である必要があります。リネームすると正しく動作しません
- セットアップを実行すると既存の `yt-dlp.exe` は上書きされます。必要に応じてバックアップを取ってください
- cookie設定は捨て垢などで行うことを推奨します
