# osecpu-vm-rev2
OSECPU Virtual Machine (Rev.2)

WindowsおよびOSX上でのコンパイルに対応したOSECPU-VMです。

http://osecpu.osask.jp/wiki/?page0072 で配布されているパッケージをもとに、いくつか修正を加えています（主にマルチプラットフォーム対応）。

## Build environment
### Windows
- gcc6.3.0 (MinGW) で検証済み

### OSX
- LLVM 8.1.0 (clang-802.0.42) で検証済み

### How to build

- ルートディレクトリで `make` すると、以下の実行ファイルおよびOSECPUアプリが生成されます。

  - `vm/osecpu` (`vm/osecpu.exe`) : VM本体

  - `tol/osectols` (`tol/osectols.exe`) : ツールチェイン

  - `app/*.ose` : OSECPUアプリ（バックエンドコード）

  - `app/*_.ose` : OSECPUアプリ（フロントエンドコード）
  
## Run app on OSECPU-VM
### Windows
```
vm¥osecpu.exe app¥app0100.ose
```
### OSX
```
vm/osecpu app/app0100.ose
```

## Contributors
- 川合秀実 : 下記以外のすべて
- Livaさん : DEP(DataExecProtect)対策, VMのMacOS版開発 (2011年セキュリティ＆プログラミングキャンプ受講生)
- Irisさん : 優良アプリ開発, VMバグ発見(rev1), VM改良提案 (2013年セキュリティキャンプ受講生)
- takeutch-kemecoさん : VMのLinux版開発, フォーク元リポジトリの管理
- herbaさん : VMバグ発見(rev1)
- lambdaliceさん : VMバグ発見(rev1) (2013年セキュリティキャンプ受講生)
- hikarupspさん(hikalium) : VMのMacOS版改良パッチ, CMPcc命令のbit1の仕様についてアイデア提供, マルチプラットフォーム対応
- ???さん : VMのMacOS版改良パッチ, 優良アプリ開発, VM脆弱性指摘, VM改良案 (2014年セキュリティキャンプ受講生)
- 水上拓哉さん : VM脆弱性指摘 (2014年セキュリティキャンプ受講生)
- int512さん : VM脆弱性指摘 (2014年セキュリティキャンプ受講生)

## App list
-  app0100 (  13バイト) : グラデーション
-  app0101 (  14バイト) : 日本の国旗
-  app0102 (  14バイト) : XOR描画のテスト
-  app0103 (  63バイト) : bballアプリ
-  app0104 (  24バイト) : メタリックなボール
-  app0107 ( 430バイト) : インベーダーゲーム (改良の余地あり)
-  app0114 (  51バイト) : バウンドするボール
-  app0115 (  19バイト) : XOR演算によるきれいな模様 by ???
-  app0116 (  83バイト) : Cカーブ描画アプリ by ???
-  app0117 (  68バイト) : app0116のサイズ最小版
-  app0118 (   8バイト) : app0100のサイズ最小版
-  app0119 (   9バイト) : app0115のサイズ最小版
-  app0124 (   6バイト) : 世界最小グラデーション (page0103参照)
-  app0125 (  88バイト) : コルモゴロフ複雑性対決 (page0103参照) by ??? & K
-  app0126 ( 111バイト) : コルモゴロフ複雑性対決 (page0103参照)
-  app0127 (  49バイト) : マンデルブロ集合対決 (page0103参照) by ??? & K
-  app0129 ( 104バイト) : hexdump
-  app0132 (3293バイト) : OSASKの壁紙を表示するアプリ
-  app0133 ( 579バイト) : 128x128の画像を表示するアプリ
-  app0134 ( 152バイト) : BMPファイルビューア（picviewのプラグインにもなる）
-  app0136 ( 151バイト) : app0134のモノクロ版
-  app0140 (  11バイト) : 「ランダムな色で埋め尽くすとたいていは灰色になる」を確かめるアプリ
-  app0143 ( 140バイト) : ライフゲーム by 水上拓哉さん

## Changelog
ver.1.11:
  フロントエンドコードのデコーダがPLIMM命令やLMEM命令に対応。

ver.1.12:
  sleepやinkeyのAPIに対応。レジスタの使い方の変更に対応。CMPcc命令のbit1の意味を拡張。

ver.1.13:
  Enter/Leave命令に対応。tek5に対応。MacOS版の表示周りを改善。

ver.1.14:
  talloc, tfree, malloc, mfree命令をとりあえず実装。簡易デバッガを追加。
  vm-miniを作成（VMからフロントエンドコードのサポート、デバッグ機能、セキュリティチェックを
    取り去ったもの）。

ver.1.15:
  vm-m32を追加。osectolsにb32ツールを追加。

ver.1.16:
  Enter/Leave, talloc/tfreeに関するバグを修正。

ver.1.17:
  osectolsのdb2binがREM38()に対応。VMにapi_putcharやapi_putStringを追加。
  このバージョンでは、vm-miniとvm-m32が機能的に追いついていません（osecpu118dで追いつかせます）。

ver.1.18:
  vm-miniとvm-m32を、vmに追いつかせました。

ver.1.19:
  フロントエンドコードの自動生成を開発開始。

ver.1.20:
  フロントエンドコードが関数呼び出しに対応。でもまだまだです。

ver.1.21:
  フロントエンドコードがメモリアクセスに対応。MacOS版はキー入力に対応。

ver.1.22:
  for構文のcontinue命令の仕様を変更。フロントエンドバイトコードの仕様を微調整。
  bballが63バイトを達成！

ver.1.23:
  ベクトル化プリフィクス対応命令を増やしました。

ver.1.24:
  フロントエンドバイトコードが一部の浮動小数点演算命令に対応。
  ファイル入出力に暫定対応。
  フロントエンドバイトコードを改良。

ver.1.25:
  フロントエンドバイトコードがputStringやdrawStringに対応。

ver.1.26:
  OSECPU-VMをC言語からセキュアなプラグインとして利用できるように整備開始。

ver.1.27:
  picviewがデバッグモニタを起動してしまう深刻なバグがあったのでそれを修正。

ver.1.28:
  乱数APIを追加。さらにいくつかの脆弱性をやっつけた。

ver.1.29:
  プラグインに不正なフロントエンドコードを書いても、正しくdownと表示されて処理が続行するようになった。

ver.1.30:
  api_drawLineのアルゴリズムをブレゼンハムのアルゴリズムに変更。

ver.1.31:
  mfreeがまともな実装になった。ライフゲームをバンドルした。

ver.1.32:
  filtlib/oseconvを作り始めた。

