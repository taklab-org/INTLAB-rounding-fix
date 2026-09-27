# INTLAB-rounding-fix

[English](README.md)

Apple Silicon版MATLAB R2026aとINTLABで観測した、Apple Accelerate BLASの方向付き丸めの不具合を補う実験パッチです。

**パッチ本体はApple Accelerate向けのC動的ライブラリです。** MATLAB／INTLABは調査のきっかけとなった利用例であり、パッチ本体はどちらにも依存しません。ビルド・MATLAB起動・試験実行にはBash、MATLAB／INTLAB内の検証にはMATLAB関数を使います。Pythonは不要です。

各BLAS計算コールバックへ呼出し元の丸め方向を渡し、終了後にそのスレッドの元の方向へ戻します。**Accelerate自身の並列計算カーネルを使います。** ABI 2ではSME非搭載機の計算経路をCPU側へ切り替えます。 行列積を単一スレッドへ変更する方式ではありません。

Apple／MathWorks／INTLABの公式修正ではなく、公開SDK外の入口を使います。OSやMATLABの更新後には再検証してください。[検証範囲と制限](docs/validation.md)

## M3 Max対応の修正（ABI 2）

旧ABI 1は今回のM3 Maxでは読み込みに成功しても包含検査に失敗しました。ABI 2は、BLASに限定した計算経路の選択と、`dispatch_apply`への丸め伝播を追加しています。dylibとMEXを再ビルドし、`scripts/matlab.sh`で新しいMATLABを起動してください。起動済みプロセスの計算経路は変更できません。

M3 Max＋macOS 26.6.2＋R2026a Update 2＋INTLAB V13では、実doubleの行列積について、2つのprocess workerを含む検査を通過しました。Update 2全般の不具合、あるいはINTLABの全演算の安全性を示すものではありません。CPU経路への切替には速度上の代償があります。[原因の切り分けと検証記録](docs/m3-max-validation.md)

## 自動選択と既存版の更新

ランチャーがMATLAB起動前にパッチを読み込み、パッチがCPU機能に応じてBLASの経路を自動選択します。CPU名の指定や、利用者による `DYLD_INSERT_LIBRARIES` の設定は不要です。

| 検出した機能 | 自動処理 |
|---|---|
| SME/SME2なし・アクセラレータ選択あり（実測したM3 Max） | AccelerateのCPUカーネルを選び、並列コールバックへ丸め方向を伝播 |
| SME/SME2あり | Accelerate本来の選択を維持し、コールバックへ丸め方向を伝播 |
| アクセラレータ選択なし | 既存の選択を維持し、コールバックへ丸め方向を伝播 |

この表は機能による分岐を示すもので、各分類の全CPUでの数値保証を意味しません。ABI 2の数値検証は記載のM3 Max環境で実施済みです。SME分岐の選択ロジックは回帰テストで確認しますが、M4実機での再検証は未実施です。

既存のリポジトリを更新する場合は、そのルートで次を実行してください。

```sh
git pull --ff-only
./scripts/build.sh
./scripts/test_native.sh
./scripts/matlab.sh --check
./scripts/matlab.sh
```

Gitの更新だけではビルド済みdylib/MEXは更新されないため、両方の再ビルドが必要です。旧ABI 1のビルド情報が残っている場合、ランチャーは再ビルドを案内して停止します。今回のM3 Maxでは `--check` が `ABI=2 cpuFallback=1` と上下方向の違反0件を表示します。`cpuFallback=0` は本来の経路を維持したことを表します。いずれも数値包含検査を通過したことを確認し、このランチャーから新しい計算プロセスを起動してください。

## 必要な環境

- Apple Silicon Mac、macOS標準のBash、使用可能なApple Command Line ToolsまたはXcodeとmacOS SDK。
- MATLABで使用する場合はApple Silicon版MATLAB。ABI 2の検証環境はM3 Max、macOS 26.6.2、R2026a Update 2です。旧ABI 1はM4 Pro＋Update 5で検証しています。ABI 2のM4での再検証は未実施です。
- INTLAB試験を行う場合だけ、別途用意したINTLAB。

MATLAB・INTLAB・Appleのライブラリ本体やヘッダは同梱していません。ビルド成果物と環境固有の設定・ログは、Git管理対象外の`build/`へ保存します。

## ビルドと起動

リポジトリを取得し、ルートで実行します。

```sh
git clone https://github.com/taklab-org/INTLAB-rounding-fix.git
cd INTLAB-rounding-fix
./scripts/build.sh
./scripts/test_native.sh
./scripts/matlab.sh --check
./scripts/matlab.sh
```

最後のコマンドでパッチ付きMATLABデスクトップを起動します。バッチ処理には `./scripts/matlab.sh -batch "your_function"` を使用できます。`-sd`を指定しなければ呼出し元の作業ディレクトリを維持します。通常の起動では利用者の`startup.m`を置き換えず、INTLABも自動初期化しません。`--check`だけは何もしないstartupを持つ診断用ディレクトリで実行します。

MATLABは `/Applications/MATLAB_R2026a.app` を優先して検出し、なければ `/Applications` 内の候補が一つの場合に使用します。別の場所なら次のように指定します。

```sh
./scripts/build.sh --matlab "/path/to/MATLAB.app"
```

`MATLAB_ROOT`環境変数も使えます。MATLABを変更したらMEXを再ビルドしてください。コンパイラとSDKは`xcrun`で検出し、必要なら `--cc` と `--sdk` で指定します。自動インストールやライセンスへの自動同意は行いません。

MATLABの場所は`build/matlab-root.txt`にデータとして保存します。起動時の`--matlab`でも上書きできます。旧Python版から更新した場合は、最初に`./scripts/build.sh`で再ビルドしてください。

## 各計算プロセスでの確認

パッチ付きMATLABで、実際に使うBLASスレッド設定のもと、次を実行します。

```matlab
report = rounding_patch_check(512);
report.patch
```

実際のBLAS、パッチの読込み、厳密な行列積の上下方向を検査します。元の丸め方向は終了時に復元します。BLAS名はパッチなしと同じなので、それだけでは成功判定できません。INTLABの起動時検査だけでも不十分です。

process workerも**各worker自身で**確認してください。単一スレッドでの成功は並列時の保証にはなりません。OS・MATLAB・パッチ・並列条件を変更した場合は、使用するサイズも含めて検証します。通常のINTLAB／正式計算bootstrapとキャッシュ所有規約は引き続き利用者のプロジェクトで管理してください。

## 再現試験

MATLAB不要のC試験だけをビルドする場合:

```sh
./scripts/build.sh --native-only
./scripts/test_native.sh
```

パッチなしでの失敗を記録し、パッチありでの包含成功とBLAS workerへの介入を確認します。別環境でパッチなしでも成功する可能性はあるため、元の失敗をすべてのMacの必須条件にはしていません。

Accelerateを使うネイティブ実行ファイルにも、Bashから実行直前にパッチを指定できます。上記ビルド後の例です。

```sh
(export DYLD_INSERT_LIBRARIES="$PWD/build/libaccelerate_rounding.dylib"; exec ./build/witness)
```

試験ランナーでは監査カウンタを有効にして、workerコールバックの重なりも確認します。他のアプリでは動的ライブラリの読込みが許可される必要があり、保護された起動プログラムを経由すると`DYLD_*`が除去される場合があります。アプリごとに実際の数値包含とworkerへの介入を検証してください。MATLABの起動シェルは専用ランチャーで対応しています。

MATLAB対応でビルドした後、MATLAB試験を実行します。

```sh
./scripts/build.sh
./scripts/matlab.sh -sd "$PWD/tests" -batch run_matlab_tests
```

INTLABも含める場合だけ、専用のruntimeを明示します。

```sh
INTLAB_ROOT="/path/to/private/Intlab" \
  ./scripts/matlab.sh -sd "$PWD/tests" -batch run_matlab_tests
```

この指定は`startintlab`を呼ぶため、INTLABのキャッシュを書き換えることがあります。他のジョブが使用・初期化中の共用runtimeは指定しないでください。原文の`testmm(288/512/540/1024)`と、実際の区間端点による包含検査を行います。未指定時はINTLAB試験を明示的に省略します。

介入回数などの診断には `ACCELERATE_ROUNDING_AUDIT=1 ./scripts/matlab.sh --check` を使えます。通常は計数しないため、統計がゼロでも未読込みとは限りません。

## 適用範囲と解除

パッチは `libBLAS.dylib` からの `dispatch_apply_with_attr` と `dispatch_apply` を対象にし、各呼出しに固有の丸め情報を渡します。SME非搭載機では、同じくBLASからの `_get_cpu_capabilities` 呼出しに限って内部のアクセラレータ選択ビットをマスクし、AccelerateのCPU並列経路を選びます。選択はプロセス内でキャッシュされ、最近接丸めの積にも適用されます。SME/SME2搭載機は従来の選択を維持する設計ですが、ABI 2をM4で再検証した結果はまだありません。`rounding_patch_check` の `report.patch.cpuFallbackSelected` で選択を確認できます。異なる方向で同時に積を計算しても、一つのグローバル変数を上書きし合う設計ではありません。

ランチャーはMATLAB起動時だけBLASとこのパッチを指定します。ホームの起動設定、launchdの共通環境、MATLAB本体、OSライブラリ、署名は変更しません。他のinterposerとの同時使用は検証していません。

このリポジトリのパッチを使わなくするには、そのMATLABを終了し、通常の方法で起動します。別途設定済みの恒久設定は自動解除しません。Finder／Dockのアイコンにも自動適用されません。

実doubleのDGEMMを中心とした試験であり、全BLAS経路・全データ型・非正規数等の極端な入力・INTLAB全体の認証ではありません。公開SDK外のABIや内部経路が変わると適用できなくなる可能性があります。丸め方向以外の浮動小数点制御や例外フラグの集約も対象外です。

M4 Airでの動作確認は利用者から報告されていますが、OS・MATLAB版と詳細ログは未取得です。互換性報告には環境・BLAS名・スレッド上限・サイズ・パッチの版を添えてください。ログに含まれる個人パスは公開前に除いてください。

本プロジェクトのコードは[MIT License](LICENSE)で提供します。外部依存の条件は[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)を参照してください。
