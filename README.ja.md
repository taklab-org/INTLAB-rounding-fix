# INTLAB-rounding-fix

[English](README.md)

Apple Silicon版MATLAB R2026aとINTLABで観測した、Apple Accelerate BLASの方向付き丸めの不具合を補う実験パッチです。

各BLAS計算コールバックへ呼出し元の丸め方向を渡し、終了後にそのスレッドの元の方向へ戻します。**Accelerate自身の行列分割・計算カーネル・並列処理を維持します。** 行列積を単一スレッドへ変更する方式ではありません。

Apple／MathWorks／INTLABの公式修正ではなく、公開SDK外の入口を使います。OSやMATLABの更新後には再検証してください。[検証範囲と制限](docs/validation.md)

## 必要な環境

- Apple Silicon Mac、Python 3.9以上、使用可能なApple Command Line ToolsまたはXcodeとmacOS SDK。
- MATLABで使用する場合はApple Silicon版MATLAB。詳細な検証環境はM4 Pro、macOS 26.6.2、R2026a Update 5です。
- INTLAB試験を行う場合だけ、別途用意したINTLAB。

MATLAB・INTLAB・Appleのライブラリ本体やヘッダは同梱していません。ビルド成果物と環境固有の設定・ログは、Git管理対象外の`build/`へ保存します。

## ビルドと起動

リポジトリを取得し、ルートで実行します。

```sh
git clone https://github.com/taklab-org/INTLAB-rounding-fix.git
cd INTLAB-rounding-fix
python3 scripts/build.py
python3 scripts/test_native.py
python3 scripts/matlab.py --check
python3 scripts/matlab.py
```

最後のコマンドでパッチ付きMATLABデスクトップを起動します。バッチ処理には `python3 scripts/matlab.py -batch "your_function"` を使用できます。`-sd`を指定しなければ呼出し元の作業ディレクトリを維持します。通常の起動では利用者の`startup.m`を置き換えず、INTLABも自動初期化しません。`--check`だけは何もしないstartupを持つ診断用ディレクトリで実行します。

MATLABは `/Applications/MATLAB_R2026a.app` を優先して検出し、なければ `/Applications` 内の候補が一つの場合に使用します。別の場所なら次のように指定します。

```sh
python3 scripts/build.py --matlab "/path/to/MATLAB.app"
```

`MATLAB_ROOT`環境変数も使えます。MATLABを変更したらMEXを再ビルドしてください。コンパイラとSDKは`xcrun`で検出し、必要なら `--cc` と `--sdk` で指定します。自動インストールやライセンスへの自動同意は行いません。

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
python3 scripts/build.py --native-only
python3 scripts/test_native.py
```

パッチなしでの失敗を記録し、パッチありでの包含成功とBLAS workerへの介入を確認します。別環境でパッチなしでも成功する可能性はあるため、元の失敗をすべてのMacの必須条件にはしていません。

MATLAB対応でビルドした後、MATLAB試験を実行します。

```sh
python3 scripts/build.py
python3 scripts/matlab.py -sd "$PWD/tests" -batch run_matlab_tests
```

INTLABも含める場合だけ、専用のruntimeを明示します。

```sh
INTLAB_ROOT="/path/to/private/Intlab" \
  python3 scripts/matlab.py -sd "$PWD/tests" -batch run_matlab_tests
```

この指定は`startintlab`を呼ぶため、INTLABのキャッシュを書き換えることがあります。他のジョブが使用・初期化中の共用runtimeは指定しないでください。原文の`testmm(288/512/540/1024)`と、実際の区間端点による包含検査を行います。未指定時はINTLAB試験を明示的に省略します。

介入回数などの診断には `ACCELERATE_ROUNDING_AUDIT=1 python3 scripts/matlab.py --check` を使えます。通常は計数しないため、統計がゼロでも未読込みとは限りません。

## 適用範囲と解除

パッチは `libBLAS.dylib` からの `dispatch_apply_with_attr` だけを対象にし、各呼出しに固有の丸め情報を渡します。異なる方向で同時に積を計算しても、一つのグローバル変数を上書きし合う設計ではありません。

ランチャーはMATLAB起動時だけBLASとこのパッチを指定します。ホームの起動設定、launchdの共通環境、MATLAB本体、OSライブラリ、署名は変更しません。他のinterposerとの同時使用は検証していません。

このリポジトリのパッチを使わなくするには、そのMATLABを終了し、通常の方法で起動します。別途設定済みの恒久設定は自動解除しません。Finder／Dockのアイコンにも自動適用されません。

実doubleのDGEMMを中心とした試験であり、全BLAS経路・全データ型・非正規数等の極端な入力・INTLAB全体の認証ではありません。公開SDK外のABIや内部経路が変わると適用できなくなる可能性があります。丸め方向以外の浮動小数点制御や例外フラグの集約も対象外です。

M4 Airでの動作確認は利用者から報告されていますが、OS・MATLAB版と詳細ログは未取得です。互換性報告には環境・BLAS名・スレッド上限・サイズ・パッチの版を添えてください。ログに含まれる個人パスは公開前に除いてください。

本プロジェクトのコードは[MIT License](LICENSE)で提供します。外部依存の条件は[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)を参照してください。
