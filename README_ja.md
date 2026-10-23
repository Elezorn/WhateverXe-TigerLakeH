# WhateverXe

[English](README.md)

WhateverXeは、一部のIntel Xe-LP内蔵GPU向けにAppleのグラフィックスドライバを適応させるLiluプラグインです。WhateverGreenを基に開発しています。

本プロジェクトは実験段階であり、[WhateverXe Graphics](https://github.com/b00t0x/WhateverXe-Graphics)の署名済みグラフィックスpkgが必要です。資材を`/Library`へ配置してAuxiliary Kernel Collection（AuxKC）を更新し、Systemファイルやsnapshotにはパッチを適用しません。WEXとグラフィックスpkgは、それぞれ独立したバージョンでリリースします。

## 対応範囲

単一のWEXバイナリと1つのグラフィックスpkgで、以下のmacOSに対応します。

| macOS | 必要バージョン |
| --- | --- |
| Big Sur | 11.7.11 |
| Monterey | 12.7.6 |
| Ventura | 13.7.8 |
| Sonoma | 14.8.9 |
| Sequoia | 15.8.x |
| Tahoe | 26.7以上 |

Big Sur〜Sonomaは検証済みカーネルの識別を必要とします。SequoiaとTahoeでは、実行時の命令列・ABI検査を維持した上で、同じOS系列内の更新を受け付けます。将来の更新が検証済みという意味ではありません。

WEXはOSのABIとGPUバックエンドを独立して選択します。OS別のグラフィックス資材と共通のAppleIntelTGLCompatはWhateverXe Graphicsが導入し、GPU世代・コネクタ配線への対応はWEXが担います。

| GPUファミリー | バックエンド実装があるdevice ID |
| --- | --- |
| Tiger Lake | `9A40`、`9A49`、`9A78` |
| Alder Lake-N / Twin Lake | `46D0`–`46D4` |

対応状況は機種やコネクタ構成によって異なります。この表はバックエンドの実装範囲を示すもので、すべての機種での動作を保証するものではありません。詳細な選択条件と上書き設定は[デバイス選択](docs/device-selection.md)を参照してください。

## 必須構成

[Releases](https://github.com/b00t0x/WhateverXe/releases)から **RELEASE** ZIPをダウンロードしてください。DEBUG ZIPは問題調査用です。各ZIPには`WhateverXe.kext`と対応するdSYMのみを収録し、両ZIPのチェックサムは`SHA256SUMS`で配布します。各版の変更点は[CHANGELOG](CHANGELOG.md)を参照してください。

| コンポーネント | 役割 |
| --- | --- |
| Lilu | WhateverXeの動作に必要なランタイム |
| WhateverXe.kext | OpenCoreでLiluより後に読み込む |
| [WhateverXe Graphics pkg](https://github.com/b00t0x/WhateverXe-Graphics/releases) | 署名済み資材を`/Library`へ導入しAuxKCを更新する |
| [AMFIPass v1.4.1 Mod](https://github.com/b00t0x/WhateverXe-Graphics/blob/main/extras/AMFIPass-v1.4.1-Mod-RELEASE.zip) | 改変されたグラフィックス構成に必要。OpenCoreで導入する |

SIPは`csr-active-config=03080000`（値`0x803`）に設定してください。SIPの全面有効化には対応していません。署名済みグラフィックスpkgとリンク先のMod AMFIPassを使用する場合、`amfi=0x80`と`ipc_control_port_options=0`は不要です。

Tahoeでは`CoreMedia / AllowOnePassMetalScaling`の無効化が必要です。WhateverXe Graphicsのインストーラーが設定します。

グラフィックスpkgはWEX **0.3.0以上**を読み込んだ対象OSから実行してください。旧Xe System patchがある場合は、先に元の復元手順で戻してください。pkgはEFI・Systemファイル・snapshot・SIP設定を変更しません。pkgの使用方法と単独のアンインストールスクリプトは[導入と削除](https://github.com/b00t0x/WhateverXe-Graphics/blob/main/README_ja.md#インストール)を参照してください。

リンク先の **Mod版** アーカイブから`AMFIPass.kext`を展開し、`EFI/OC/Kexts`へ配置して、`Kernel > Add`で有効にしてください。既存のAMFIPassがある場合はこのMod版へ置き換え、両方を同時に読み込まないでください。upstreamのAMFIPassはMod版の署名チェーンに対応していません。

他のroot patchにOCLPを併用する場合は、同じMod版の署名チェーンを使用する[独自ビルド版OCLP](https://github.com/b00t0x/OpenCore-Legacy-Patcher)を使用してください。Xeグラフィックスの導入自体はOCLPに依存せず、WhateverXe Graphicsを使用します。

複数GPUの構成を含め、WhateverGreenとWhateverXeは同時に有効にしないでください。

## 設定

GPUはPCI device IDから自動選択します。対応範囲内ではファームウェアのVBTから配線を読み取るため、固定HDMI／固定DisplayPort構成は通常、配線propertyの手動指定が不要です。高度な設定としての`device-id`上書きは[デバイス選択](docs/device-selection.md)で説明しています。

対応GPUでは、WhateverGreenと同様に、検出されたIntel Management Engineのデバイス名も`IMEI`へ統一します。`HECI`や`MEI`から`IMEI`へのACPI renameを別途追加する必要はありません。

### 内蔵パネルの輝度

内蔵ディスプレイの輝度調整には、OpenCoreへ`SSDT-PNLF`を追加してください。

### DVMT

BIOS/UEFIで変更できる場合は、**DVMT Pre-Allocatedを96MBに設定することを推奨します**。

**60MBから変更できない場合**は、`boot-args`に`-igfxdvmt`を追加し、OpenCoreの`DeviceProperties > Add`にあるiGPUの項目（通常は`PciRoot(0x0)/Pci(0x2,0x0)`）へ以下の2つを設定してください。

| Property | 型 | 値（16進数） |
| --- | --- | --- |
| `framebuffer-patch-enable` | Data | `01000000` |
| `framebuffer-stolenmem` | Data | `0000C003` |

Data値はリトルエンディアンです。`-igfxdvmt`は容量計算を補正し、DevicePropertiesは表示用メモリ領域を60MB内に収めます。ファームウェアが確保したメモリを増やす設定ではありません。96MBの場合、これらのDVMT用設定は不要です。

### 必須ブート引数

OpenCoreの`boot-args`に以下を追加してください。

```text
-allow3d -disablegfxfirmware
```

| 引数 | 用途 |
| --- | --- |
| `-allow3d` | Appleドライバの3D処理を有効にする。 |
| `-disablegfxfirmware` | 現在のバックエンドで、Appleドライバのグラフィックスファームウェア読み込みを無効にする。 |

`-wex-nopanel`ブート引数は廃止され、不要になりました。内蔵ディスプレイの有無は自動で判定します。

Alder Lake-N / Twin Lakeで世代選択や個別機能の有効化引数を追加する必要はありません。
GPU backendの選択と実機検証範囲は独立しています。未検証のPCI revisionも同じfamily実装を使い、
文書化された差分や実際の不具合で必要になった場合にrevision別の処理を追加します。

対応済みのDisplay 13バックエンドではYタイル出力を自動選択するため、OpenCoreの`AAPL,GfxYTile`設定は不要です。既存の値は保持します。
このバックエンドはXタイル出力に未対応なので、Appleドライバの`-yTileDisable`引数は使用しないでください。Display 12のタイル選択は変更しません。

## 動作確認済みの機能と既知の問題

| ハードウェア | 確認済みの機能 |
| --- | --- |
| Tiger Lake / Alder Lake-N | Big Sur〜Tahoe：外部2画面の1920×1080・60Hz出力、Metal/MPS、OpenGL、OpenCL、ハードウェアH.264エンコード／デコード、HEVCデコード、画面の消灯／復帰、Safariの動画デコード、Widevine再生 |
| Tiger Lakeラップトップ | Big Sur〜Tahoe：内蔵1366×768 eDP、輝度調整、HDMI、USB-C DisplayPort Alt Mode、最大3画面同時出力。TahoeではHDMIとUSB-Cの2端子で抜き差しも確認 |
| Alder Lake-N | Big Sur/Tahoe：EFIの`AAPL,GfxYTile`設定なしでのYタイル自動選択 |

検証した機器での結果であり、すべてのGPU・ディスプレイ・アダプター・ドックでの動作を保証するものではありません。

Big Sur〜Tahoeでは、有効なApple platform署名とサービス識別子でWebKit GPUを判定し、
Safariのインストール先や更新による配置変更に依存せずData側GPUドライバへのアクセスを扱います。

### 既知の問題と制限

- 一部の画像変換処理で、アプリケーションがクラッシュすることがあります。
- DisplayPort専用端子でモニターを接続し直すと、黒画面になったりコンピューターが応答しなくなったりすることがあります。USB-CのDisplayPort Alt Modeでは、上記の抜き差し試験を通過しています。
- Safariの動画再生で、ときどきコマ落ちが発生します。
- 同時出力は最大3画面です。4画面目には映像が出ません。
- 開発版ではTGL Combo A/Bの固定外部DPもVBTから配線を生成し、他のDP経路とSST訓練・電源・AUX保持を共有します。この組合せはCPU試験済みで実機未検証です。AUXの交差配線やType-C/TBT管理を必要とするCombo配線は除外します。
- TGL eDPの信号設定は、物理GPUのsubplatformとVBTのpanel情報からRBR〜HBR3を選択します。HOBLを要求するfirmwareでも標準のLinux fallback係数を使用します。HOBL最適化は再試行処理を実装するまで無効です。高速度の選択はCPU試験済みですが、上記の実機結果は全link rateの動作保証ではありません。
- 資格を満たすTGLのPWM/PPS 0 panelでは、VBTの待ち時間・firmwareの初期値・明示panelプロパティを統合し、nativeの電源シーケンスを維持します。新しい待ち時間adapterはCPU・実donorコードで検証済みです。他のbacklight方式やPPS controllerはこのadapterの対象外です。
- システム全体のスリープ／復帰と、上記を超える解像度は未検証です。USB-C映像出力では現在、表示回路の一部を省電力状態へ移行させずに動作させています。

## ロードマップ

### Planned

- Alder Lake／Raptor LakeのS/H/P/U対応。開発版のAlder Lake-S `4690`では、
  Big Sur〜TahoeでMetal／OpenGLアクセラレーションと3画面同時表示（1080p60×2＋720p60×1）を確認しています。
  更新版VA資材との組み合わせでは、Big SurとTahoeでハードウェアH.264エンコード／デコード、
  HEVCデコード、Safariのハードウェア動画デコード、Widevine再生も確認しています。
  未公開のグラフィックス資材変更が必要で、公開済みpkgでは対応しません。
  Big Surで固定DPの再接続は確認済みですが、起動時の表示や再接続後の復帰に時間がかかる場合があります。
  HDMI／固定DPの配線は機種・コネクタのプリセットではなくVBTから生成し、現在のバックエンドはCombo PHY A～Cを扱います。
  曖昧な配線、DDIと異なるAUXへの配線、Type-C／TBT機能、PHY D／Eは除外します。
  開発版のGPU受付は、ADL-S `4680/4682/4688/468A/468B/4690/4692/4693` と、
  RPL-S `A780/A781/A782/A783/A788/A789/A78A/A78B` が対象で、すべてのPCI revisionが共通backendを選択します。
  物理IDの一致、1 slice・最大2 DSS・16/24/32 EU・4 L3 bank・対応media engine構成を要求し、
  この実装範囲外の資源構成は未対応です。実機確認済みは `4690` revision `0C` のみで、他IDはCPU試験の範囲です。
  追加IDには更新版のcompanion側ID受付も必要です。
  別のポート配置はCPUテストで確認していますが、実機では未検証です。
  VBT上でCombo PHY Aの固定DPとして扱われるUSB-C端子でも実表示を確認しています。
  これはType-C/TBT管理経路の動作確認ではありません。開発版のeDP経路は、VBTで確認したDDI Aのpanelを
  native PPS0／PWM controller 0に接続し、panel timing、信号設定、firmware由来のPWM周期・極性を適用します。
  この経路はCPU試験と実donor再生で検証していますが、ADL-S eDPの実機検証は未実施です。
  PPS1とAUX制御のbacklightは未対応です。消灯／復帰は未検証です。
- Display 13のUSB-C / eDP対応

### Not Planned

- Xe-LPG対応
- Xe2以降のアーキテクチャへの対応

## ビルドとテスト

macOS、Xcode command-line tools、Python 3.9以降、MacKernelSDK、Liluの開発用ヘッダ／ライブラリを使用します。CIではTahoeのkernel定義を含むLiluの[1.7.2リリースタグ](https://github.com/acidanthera/Lilu/tree/1.7.2)を使用します。`MacKernelSDK/`と`Lilu.kext/`をリポジトリ直下に配置するか、`--sdk`と`--lilu`でパスを指定してください。

```sh
python3 tools/build.py --release
python3 tools/test.py
```

WhateverXeとAppleIntelTGLCompatのkext、およびソースのmanifestは`build/`に出力されます。TGLCompatのソースと共有ヘッダーはこのリポジトリで管理し、署名済みバイナリはWhateverXe Graphicsで配布します。WEXのRelease ZIPに含まれるkextはOpenCore用プラグインのみで、グラフィックスpkgは含みません。任意のdonor再生試験には`--donor /path/to/AppleIntelTGLGraphicsFramebuffer`と`--accelerator /path/to/AppleIntelTGLGraphics`を指定します。各試験が受け付ける正確なバイナリが必要です。CPUテストだけで実GPUの互換性が確認できるわけではありません。ビルドにWhateverGreenのソースは不要です。

## ライセンスとクレジット

WhateverXeはWhateverGreenの著作権表示とBSD-3-Clauseライセンスを継承しています。[LICENSE.txt](LICENSE.txt)を参照してください。LiluとMacKernelSDKは別の依存コンポーネントです。
