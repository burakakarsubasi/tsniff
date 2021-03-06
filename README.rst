
ABOUT
=====

これは、所謂 TS 抜きを Linux で行うソフトウェアです。

作成にあたって、以下のソフトウェアを利用・参照させて頂きました。
ありがとうございます。

* デジ太郎氏の cap_sts_ 2008_03_18 版
* TeamKNOx氏の `arib_std_b25 改造版`_
* 茂木 和洋氏の `ARIB STD-B25 仕様確認テストプログラムソースコード`_
* ◆N/E9PqspSk氏の Linux関係詰め合せ_ (up0281.zip)

.. _cap_sts: http://optimize.ath.cx/cusb_fx2/r_and_d.html
.. _arib_std_b25 改造版: http://www.teamknox.com/DigitalWare/YDBC-30j.html
.. _ARIB STD-B25 仕様確認テストプログラムソースコード: http://www.marumo.ne.jp/db2008_3.htm#24
.. _Linux関係詰め合せ: http://2sen.dip.jp/friio/


INSTALL
=======

必要なもの
----------

* python >= 2.3
* glib 2.14
* libusb-1.0 >= 1.0.0
* libpcsclite

1. tsniff を入手します::

   $ hg clone http://atty.skr.jp/hg/tsniff

2. 依存ライブラリをインストールします::

   $ sudo aptitude install libglib2.0-dev libpcsclite-dev

3. libusb-1.0 を入手し、インストールします::

   $ http://www.libusb.org/wiki/Libusb1.0
   $ git clone git://projects.reactivated.net/~dsd/libusb.git libusb-1.0
   $ cd libusb-1.0
   $ git checkout 885c2a5de69d6b7d8902bb55d6d83680a5a1a6e5
   $ ./autogen.sh
   $ ./configure
   $ make
   $ sudo make install
   $ cd ..

4. arib_std_b25-atty を入手し、tsniff のソースツリーに配置します::

   $ hg clone http://atty.skr.jp/hg/arib_std_b25-atty tsniff/extra/arib_std_b25

5. tsniff をビルド・インストールします::

   $ cd tsniff
   $ ./waf configure
   $ ./waf
   $ sudo ./waf install

MacOSX でのビルド
-----------------

MacPorts 等を利用して glib2 をインストールしてください。

libusb-1.0 と libpcsclite に依存する機能は利用できないため、
保存済みの生 TS と B-CAS データに B25 デコードを掛けるぐらいしかできません。


SETUP
=====

Ubuntu 7.10 の場合

/etc/udev/rules.d/49-recfx2.rules::

  # Friio (White)
  SUBSYSTEM=="usb", ATTRS{idVendor}=="7a69", ATTRS{idProduct}=="0001", GROUP="video"

  # Chameleon USB FX2
  SUBSYSTEM=="usb_device", ATTRS{idVendor}=="04b4", ATTRS{idProduct}=="8613", GROUP="video"
  SUBSYSTEM=="usb_device", ATTRS{idVendor}=="04b4", ATTRS{idProduct}=="1004", GROUP="video"


OPTIONS
=======

入出力の指定
------------

FILENAME には標準入力または標準出力として、 ``-`` を指定することが可能です。

-T, --ts-input=SOURCE
   TS 入力を SOURCE に設定します。SOURCE は以下の値を取ります。
   省略時のデフォルトは fx2: です。

     fx2:
       CUSBFX2 から TS を取得します。
     FILENAME
       ファイルから TS を取得します。
       ``--bcas-input=fx2:`` と併用した場合の動作は不定です。

-B, --bcas-input SOURCE
    B-CAS データ入力を SOURCE に設定します。SOURCE は以下の値を取ります。
    省略時のデフォルトは fx2: です。

      fx2:
        CUSBFX2 から B-CAS データを取得します。
      pcsc:
        libpcsclite 経由でカードリーダから B-CAS データを取得します。
        ``--bcas-output``, ``--verify-bcas-stream`` との併用はできません。
      FILENAME
        ファイルから B-CAS データを取得します。初期化の際に全ての B-CAS データを取得します。

-t, --ts-output=FILENAME
    未加工 TS を FILENAME へ出力します。

-b, --bcas-output=FILENAME
    B-CAS を FILENAME へ出力します。

-o, --b25-output=FILENAME
    ARIB STD-B25 デコーダを有効にし、デコード済み TS を FILENAME へ出力します。


リモコン制御
------------

入力が CUSBFX2 の場合のみ有効です。

--ir-base
    リモコンの制御チャンネルを 1〜3 で指定します。
    チューナー本体の設定に合わせてください。

-s, --ir-source
    入力のソースを変更します。
      - 0: 地上デジタル
      - 1: BS
      - 2: CS

-c, --ir-channel
    チャンネルを変更します。1〜12 でリモコンのボタンによるチャンネル指定となります。
    ``--ir-source`` が BS または CS かつ 3 桁指定すると、 3 桁入力によりチャンネル変更します。


CUSBFX2
-------

--fx2-id=N
    使用する CUSBFX2 の ID を指定します。デフォルトは 0 です。

--fx2-force-load
    強制的にファームウェアをロードします。

--fx2-ts-buffer-size=N
    TS 転送バッファのサイズを N バイトに変更します。デフォルトは 16384 です。

--fx2-ts-buffer-count=N
    TS 転送の同時リクエスト数を N に変更します。デフォルトは 16 です。


ARIB STD-B25
------------

--b25-round=N
    MULTI2 暗号のラウンド数を N に変更します。デフォルトは 4 です。

-S, --b25-strip
    ``--b25-output`` に NULL パケットを保存しません。デフォルトは保存します。

--b25-ts-delay
    入力ソースが CUSBFX2 であるとき、TS 入力の遅延時間を N 秒に変更します。
    デフォルトは 0.5 秒です。

--b25-bcas-queue-size
    B-CAS データ入力が CUSBFX2 であるとき、履歴として保持する鍵の数を N に変更します。
    デフォルトは 256 です。

--b25-system-key=HEX
    B25 デコードに必要となるシステム鍵を 32 バイト(64 文字)の 16 進文字列で指定します。
    ``--bcas-input=pcsc:`` を指定している場合は必要ありません。

--b25-init-cbc=HEX
    B25 デコードに必要となる初期 CBC を 8 バイト(16 文字)の 16 進文字列で指定します。
    ``--bcas-input=pcsc:`` を指定している場合は必要ありません。


その他
------

-l, --length=N
   入力ソースが CUSBFX2 であるとき、N 秒後に自動終了します。
   デフォルトはシグナルを受けるまで無限です。

--dump-bcas-init-status
   B-CAS 初期データを tsniff.conf の形式で標準出力へ書き出します。

--verify-bcas-stream
   B-CAS データの検証のみを行います。
   現在動きません。

-v, --verbose
   より詳細なメッセージを出力します。

-q, --quiet
   警告・エラー以外のメッセージ出力を抑制します。


使い方
------

CUSBFX2 から B25 デコード済みの TS を foo.ts へ出力 ::

 $ tsniff -o foo.ts
       Inputs      |             |   Outputs
    ===============|=============|===========
     CUSBFX2:TS  --|--+          |
                   |  |          |
                   |  +--> B25 --|--> foo.ts
                   |  |          |
    CUSBFX2:BCAS --|--+          |

CUSBFX2 から生 TS を foo.ets へ、B-CAS データを foo.bcs へ出力 ::

 $ tsniff -T fx2: -B fx2: -t foo.ets -b foo.bcs
       Inputs      |             |   Outputs
    ===============|=============|===========
     CUSBFX2:TS  --|-------------|--> foo.ets
                   |             |
                   |       B25   |
                   |             |
    CUSBFX2:BCAS --|-------------|--> foo.bcs

CUSBFX2 から生 TS を foo.ets へ、B-CAS データを foo.bcs へ、
B25 デコード済み TS から NULL パケットを削除しつつ foo.ts へ出力 ::

 $ tsniff -t foo.ets -b foo.bcs -o foo.ts -S
       Inputs      |             |   Outputs
    ===============|=============|===========
     CUSBFX2:TS  --|--+----------|--> foo.ets
                   |  |          |
                   |  +--> B25 --|--> foo.ts
                   |  |          |
    CUSBFX2:BCAS --|--+----------|--> foo.bcs

CUSBFX2 から生 TS を foo.ets へ、B-CAS カードリーダーを利用して B25 デコード済み TS を foo.ts へ出力 ::

 $ tsniff -B pcsc: -t foo.ets -o foo.ts
       Inputs      |             |   Outputs
    ===============|=============|===========
     CUSBFX2:TS  --|--+----------|--> foo.ets
                   |  |          |
                   |  +--> B25 --|--> foo.ts
                   |  |          |
      PCSC:BCAS  --|--+          |

保存済みの生 TS foo.ets と B-CAS データ foo.bcs から、B25 デコード済み TS を foo.ts へ出力 ::

 $ tsniff -T foo.ets -B foo.bcs -o foo.ts
       Inputs      |             |   Outputs
    ===============|=============|===========
      foo.ets    --|--+          |
                   |  |          |
                   |  +--> B25 --|--> foo.ts
                   |  |          |
      foo.bcs    --|--+          |

保存済みの生 TS foo.ets から、B-CAS カードリーダーを利用して B25 デコード済み TS を foo.ts へ出力 ::

 $ tsniff -T foo.ets -B pcsc: -o foo.ts
       Inputs      |             |   Outputs
    ===============|=============|===========
      foo.ets    --|--+          |
                   |  |          |
                   |  +--> B25 --|--> foo.ts
                   |  |          |
      PCSC:BCAS  --|--+          |

意味なし ::

 $ tsniff -t - -o foo.ts | tsniff -T - -B pcsc: -t foo.ets -o bar.ts
       Inputs      |             |   Outputs  ||    Inputs      |             |   Outputs
    ===============|=============|=========== || ===============|=============|===========
     CUSBFX2:TS  --|--+----------|--> STDOUT -||->  STDIN:TS  --|--+----------|--> foo.ets
                   |  |          |            ||                |  |          |
                   |  +--> B25 --|--> foo.ts  ||                |  +--> B25 --|--> bar.ts
                   |  |          |            ||                |  |          |
    CUSBFX2:BCAS --|--+          |            ||   PCSC:BCAS  --|--+          |


FILES
=====

$HOME/.config/tsniff.conf
  設定ファイル


NOTES
=====

tsniff は shell script や MythTV などのフロントエンドに対して、
ハードウェア制御などの処理を行うバックエンドとして利用することを想定しています。
よって、 tsniff そのものに予約などの高度な機能を追加する予定は **全くありません** 。


B-CAS 初期データ
----------------

``--b25-output`` を指定し、かつ B-CAS データ入力に ``pcsc:`` 以外を使用する場合は、
B-CAS 初期データ(システム鍵・初期 CBC)を別途指定する必要があります。
これは、カードリーダがなければ B-CAS 初期データを取得できないことからくる制限事項です。

B-CAS 初期データは、オプションで指定するか、設定ファイルに書いておきます。

tsniff.conf::

  [b25]
  system_key = 00112233445566778899AABBCCDDEEFF00112233445566778899aabbccddeeff
  init_cbc = 0011223344556677

なお、B-CAS 初期データは ``--dump-bcas-init-status`` で取得できます。 ::

 $ tsniff --dump-bcas-init-status >$HOME/.config/tsniff.conf

そのまま設定ファイルとして利用するのが良いでしょう。


動作確認環境
------------

* HP ProLiant ML115 G5 (Athlon 1640B 2.7GHz / !GB)
* Ubuntu Linux 8.10 interpid (amd64)
* YAGI DTC110
* libusb: 1.0.3
* arib_std_b25-atty: 950a18bfa838


RESOURCES
=========

* http://d.hatena.ne.jp/atty/
* http://bitcucket.org/atty303/tsniff/
* http://atty.skr.jp/hg/arib_std_b25-atty/
* http://atty.skr.jp/hg/cap_sts-atty/


AUTHORS
=======

Koji Agawa <atty303@gmail.com>


TODO
====

* 終了ステータス (エラーの有無)
* ポータビリティの向上(libusb-1.0 任せ) 
* VLC の input module にする
* mplayer の input module にする
* Friio対応
* TSの処理
