=============================================================================
sasx ver2.1 リリースノート 2003.05.21 update 2004.06.10
=============================================================================

＜概要＞

  ・音声波形ファイルを波形表示、スペクトル表示するプログラムです。

  ・波形フォーマット
    表示できるのはリニアエンコーディングのみ
    データ型は uchar,char,short,long,float,double です。
    データ型，チャンネル数，サンプリング周波数，エンディアンは、
    引数またはメニューで設定します。

  ・プレイバック
    sox をインストールしておけば、選択した範囲、あるいは選択位置から
    後ろを再生することができます。
    マウス右ボタンまたはaltキーで選択位置から又は選択範囲を再生します。
    short はそのまま、uchar,char は 256 倍して再生します。
    long,float,double は表示ゲインに合わせて増幅／減衰させて再生します。
    (2.1.19以前は音が聞けるのは short のみ)
    スペクトログラム上で、ctrlを押しながら時間と周波数の範囲を指定すれば、
    その帯域の音のみ再生します。突発雑音などを選択的に聴く事ができます
    (2.0.05 以後)
    メニューで daloop を選ぶか shift を押しながら再生するとループ再生
    ( 2.1.22 以後 )

  ・オーディオファイル(AU,WAV,AIFF等)の扱い
    WAV と AU はヘッダのみ解釈します(2.1以降)。ただし圧縮形式は未対応。
    リニアの 8bit と 16bit しか正しく表示できません。
    -tA(default)の時は上記ヘッダを解釈します。
    その他の時 -ts -tc -tf -td では、ヘッダもデータとして扱うため
    波形として見えてしまいます。

  ・プロットデータ
    -tp を指定すると xgraph 形式のテキストデータをプロットします
    1行に2カラムで x y 座標を指定したテキストファイルを入力します
    複数系列データは空行で区切ります。
    系列の先頭行に "title のように書けば グラフの凡例として簡易表示します。
    x 座標に波形のサンプル位置相当の値を与えれば、別の波形ファイルと
    同期をとることができます

  ・ファイルサイズ制限
    波形データをメモリに読み込まず、毎回ファイルにアクセスするので、
    プログラム自体にはファイルサイズの制限はありません。
    データへのアクセススピードはＯＳのディスクキャッシュに頼っています。

  ・スペクトログラム
    広帯域と狭帯域のFFTスペクトログラム表示が切り替え可能。
    MFCC 分析からのスペクトログラムも生成できます。
    それぞれの分析パラメータは引数またはメニューで指定できます。
    周波数スケールはリニアとＭｅｌが選択できます

  ・ピッチ分析
    スペクトログラム表示のオプションとして、ピッチ分析をして
    スペクトログラム上に重ねて四角 "□" で表示します。
    参考のため、ピッチ強度の指標を0Hzから100Hzの間に点"・"で表示します
    ピッチ抽出アルゴリズムはまだ十分に検討されていません。
    特に、ピッチ抽出には瞬間的な値だけを使っており、
    トラッキングのような連続性の制限を入れておりませんので、
    ゴミや不連続が生じます。参考程度に使ってください。
    2.1.06 から、複数ピッチを抽出する実験的なコードが入っています。
    FFT option メニューで pitchNum を変えて試すことができます。

  ・ラベル表示（音素ラベル、単語ラベル等）
    幾つかの形式(ATR形式,独自形式)のラベルファイルに対応し、波形に重ねて表示します。
    ATR 形式は第１層めのみ対応します。
    また簡単な編集ができます。
    【注意】UNDO はまともに動きません

  ・複数ファイルの同期
  ・複数窓に表示した異なるファイルを同期して表示することができます。
    異る周波数の波形ファイルを同時に表示し、時間で同期させられます。
    オフセットを調整して、ずらして同期をさせることもできます。

  ・スペクトル分析窓
    スペクトル分析窓を使って、波形上の選択範囲やカーソル位置の
    スペクトルを観測できます。
    LPC,LPCCEP,MFCCからスペクトルに変換して重ねて表示できます。

  ・B-tool 連携
	（2.1.28 以降この機能は削除しました）

＜動作環境＞

  ・Xlib のみを使っているのでX11R6やXorgの殆んど少しの変更でどの
    unix でも動作すると思いますが、試してないのでわかりません。
  ・1.3 までは SunOS4.1.4, HP-UX9.0.x で動作しましたが以後は未確認です。
  ・1.6 まではFreeBSD 4.3 + XFree86 3.x で開発していました。
  ・2.1.24 まではlinux(vine)で開発していました。
  ・以後しばらく cygwin + X と ubuntu で開発していました。

＜インストール方法＞
　・mkdir sasx; cd sasx
　・tar xvfz sasx.tgz
　・Makefile を編集して環境に合わせる
　・make sasx
　・make install

=============================================================================
使い方
=============================================================================

＜インストール方法＞
 sasx[$version].tgz をコピーします
 tar xvfz sasx[$version].tgz
 vi Makefile (必要があれば)
 make

＜使い方＞
 sasx [option] filename [[option] filename ...]
 sasx -h 

＜オプション＞                                          デフォルト
入力
 -c chan              チャンネル数                      1
 -t type              type={C|S|F|D|A|P|N}              N
                        C:char  
                        S:short
                        F:float
                        D:double
                        A:ascii
                        P:plot(x,y)
                        N:auto(wav,au,short)
 -e {l|b}             バイト並び {little|big} endian    little
 -f freq              サンプリング周波数(kHz)           12
表示
 -g WxH+X+Y           １つめの窓の geometry             512x128+0+0
 -b bd_width          複数窓表示する場合の縦スペース    0
 -s start,len         選択するサンプル範囲              0,0
 -v start,len         表示するサンプル範囲              全部
 -y ymin,ymax         表示する振幅範囲                  -32768,32767
 -m frameskip         フレーム目盛表示時の周期(ms)      5
 -n framesize         分析窓幅(ms)                      20
ラベル
 -l labelfile         ラベルファイル                    
 -L save=file         修正したラベルを書き込むファイル  
 -L mode={0|1|2|3}    削除/追加でラベル文字列をどうするか
                       0:削除/その前後の複製
                       1:ラベル内容をずらして詰め合わせる
                       2:(,)で分割, (,)がなければ分割しない
                       3:(,)で分割, (,)がなければ複製する
 -L std               ラベル文字を標準入力から編集可能にする
 -L type={0|1|2|3}    ラベルの種類                      0
                       0:読み込み時に自動で適当
                       1:fname start end string (seg 形式)
                       2:start string end       (ATR 形式)
                       3:fname st en pre nxt ? ? string (SR形式)
 -L arc={0|1}         ラベルの始点から終点への弧線表示  0
 -M [mopig]*          起動時の窓に対して行うキーの先行入力
 -V skip,sftskip      表示スキップ移動量(%)             100,10
スペクトログラム
 -W size,skip         広帯域スペクトル分析窓(ms)        5,2
 -N size,skip         狭帯域スペクトル分析窓(ms)        100,10
 -S spow              パワーの圧縮率(0.1強-1.0(弱)      0.35
 -S sgain             飽和する平均振幅レベル            2000
 -S color={0|1|2}     色 0:BW 1:gray 2:color            0
 -S power={0|1}       パワー表示 0:off 1:on             1
 -S fmax=freq         表示周波数範囲(Hz) (0:auto)       0
 -S fscale={0|1}      周波数スケール 0:linear 1:MEL     0
 -S pre=emphasis      プリエンファシス係数              0.95
色
 -C name=color        色 fg,bg,cs,sc,mk,dim
 -C mw=dots           マーカ幅(ドット数)                1
 -C mgn=dots          グラブ用マージン(ドット数)        5
 -C font=fontname     ASCIIフォント                     fixed
 -C font16=fontname   日本語フォント                    k14

＜マウスボタンのバインド ＠ 波形／スペクトログラム窓＞
 左ボタン       時間範囲選択
                これは、分析、再生、ズームインの時に使用されます。
 左ボタン       左端で(上下矢印カーソル)ドラッグすると縦軸を移動
 中ボタン       メニュー
 右ボタン       押している間、選択範囲内を再生
                (選択幅が０のときは選択位置から最後まで)

 Shift+左ボタン  複数窓を同期して範囲選択
 Shift+右ボタン  押している間、ループ再生

 CTRL+左ボタン  （スペクトログラム表示のみ）
                周波数と時間の矩形範囲指定
                この範囲は、再生の時に使用されます
 Ctrl+左ボタン   左端のほうで(上下矢印カーソル)ドラッグすると縦軸を伸縮

＜マウスボタンのバインド ＠ 分析窓＞
 左ボタン       縦横のスケール目盛の範囲でのドラッグで表示範囲を移動します
 Ctrl+左ボタン  縦横のスケール目盛の範囲でのドラッグで表示範囲を増減します
 中ボタン       分析パラメータのメニュー
 右ボタン       分析方法ごとの表示のＯＮ／ＯＦＦ

＜キーバインド ＠ 波形／スペクトログラム窓＞
  z   表示範囲２分の１または選択した範囲を表示
  Z   表示範囲２倍
  a   全体表示
  g   ゲイン２倍      (スペクトル表示の時は、より濃く表示されます)
  G   ゲイン１／２    (スペクトル表示の時は、より薄く表示されます)
  f   右スクロール１ページ
  F   右スクロール１／１０ページ
  b   左スクロール１ページ
  B   左スクロール１／１０ページ
 ^F  少しだけ右
 ^B  少しだけ左
 ^A  ファイルの先頭
 ^E  ファイルの末尾
  s   スケールの切り替え(SEC -> HMS -> SAMPLE -> FRAME) (FRAME間隔は-mで指定)
 ^Q  終了
 ^X  窓を閉じる
  n   同じファイルに対して新しい窓を開く
  m   モード切替え(波形←→ＦＦＴスペクトル)
  M   モード切替え(波形←→ＭＦＣＣスペクトル)
  p   スペクトル表示範囲をピッチの存在範囲に限定し、狭帯域分析します
  P   スペクトル表示範囲を全体に戻します。
  i   ピッチ抽出とピッチ強度(0-100の範囲)を表示します。(スペクトルモードのみ)
  o   パワー強度を表示します(スペクトルモードのみ)

 <<sasx1.1b,1.2b 1.3 別途 B-Tool が必要>>
  1   FFTスペクトル
  2   LPCスペクトル
  3   LPC-CEPスペクトル
  4   LPC-MELCEPスペクトル

 <<sasx1.7以降>>
  0   分析窓を開く(FFTスペクトル)

＜ラベル編集モード ＠ ラベル表示領域での操作＞
＜ラベル編集モード ＠ ラベル表示領域での操作＞
 TAB             次のラベル区間を選択
 shift-TAB       前のラベル区間を選択
 左ボタン        境界線を１回クリックで選択(■表示)
                 選択(■表示)された境界をドラッグで移動
                   区間同士のオーバーラップは許さない(隣の境界でSTOP)
                   終点と次の始点が同時刻の場合、両者を同時に移動
                 文字列を１回クリックで選択(反転表示)
                 選択された文字列を再度クリックで編集(-Lstd 指定時)
                 文字や境界線以外のラベル領域なら、そのラベル区間を選択
 SHIFT+左ボタン  選択され境界をドラッグすることで区間を挿入(追加)して移動
                   最初に右へ動かすと後ろに追加。
                   最初に左へ動かすと前に挿入
 CTRL+左ボタン   選択された境界をドラッグして移動
                   境界同士のオーバーラップを許して移動可能
                   終点と次の始点が同時刻の密着した境界を分離して移動
 ^U              編集のUNDO しかし、境界移動のみ。しかも１回
 DEL,BS          選択されている区間を削除する
                 -Lmode=1          の時、ラベル文字列は右へずれて残る
                 -Lmode=0(default) の時、ラベル文字列も削除する
                 -Lmode=2|3        の時、ラベル文字列も削除する
 !               選択したラベルの左とマージ
                 境界を選択した場合はその境界の前後でマージ
 /               選択したラベルを分割
                 -Lmode=0 の時は同じ文字列が複製される
                 -Lmode=1 の時は文字列と区間の対応がずれる
                 -Lmode=2|3の時は、"a,b,c" を "a" と "b,c" に分割
                 -Lmode=2 の時、カンマ(,)のないラベル区間は分割されない
                 -Lmode=3 の時、カンマ(,)のないラベル区間は複製される

＜キーバインド＠分析窓＞
 0  FFT 分析の ON/OFF
 9  LPC 分析の ON/OFF
 8  LPC-MEL 分析の ON/OFF
 7  LPCCEPSTRUM 分析の ON/OFF
 6  LPCMELCEPSTRUM 分析の ON/OFF
 5  MFCC 分析の ON/OFF
 e  FFT 分析時に エネルギー表示の ON/OFF
 w  指定範囲全体を１回で分析する ON/OFF
    デフォルトは -n窓幅(ms) -nスキップ(ms) で指定した窓での繰り返し平均
 h  ハミング窓／矩形窓の切り替え
 s  横軸の周波数スケール切り替え linear/mel/log
 l  波形窓に同期して再描画するかどうか ON/OFF
    （一時的なロックであって、分析モード等を変えたら同期してしまう）
 左ボタン        左スケール内なら縦軸を移動
                 下スケール内なら横軸を移動
 CTRL+左ボタン   左スケール内なら縦軸を伸縮 
                 下スケール内なら横軸を伸縮
 右ボタン        左スケール内なら縦軸をデフォルトに戻す
                 下スケール内なら横軸をデフォルトに戻す


＜ラベル形式について＞
 type=1 SEG形式
    line-foramt:
        "path start(ms) end(ms) utterance"
    path:
        コマンドラインで指定した波形ファイルの path と完全一致した窓にのみ
        そのラベルを表示します。
        type=0(Auto) の場合、path が数字で始まると type=2(ATR) と判断
        してしまいます。
    start end:
        目的の区間の開始時刻と終了時刻を %f 形式で ms で指定します。
        従って、周波数を微妙に間違えた場合(11.025kHzを11kHzとする等)、
        ファイルの後ろのほうでずれてしまいます。
    utterance:
        type=0(Auto) の場合、utterance が数字で始まると type=3(SR) と判断
        してしまいます。
        なければ勝手に数字(#1,#2,...)をつけます。
    備考:
        単語発声区間の切り出し情報などに使います。
        ラベルファイルには複数の波形ファイルの情報を書く事ができます。
        表示する波形ファイルのパスがpathと完全一致した時にラベルを表示します。

 type=2 ATRラベル形式
    line-foramt:
        "start(ms) utterance end(ms)"
    start end:
        目的の区間の開始時刻と終了時刻を %f 形式で ms で指定します。
        従って、周波数を微妙に間違えた場合(11.025kHzを11kHzとする等)、
        ファイルの後ろのほうでずれてしまいます。
    utterance:
        当然ながらスペースがあるとだめです。
    備考:
        ATR ラベル形式は、次の５つの層が #のみの行で区切られています。
           { 音韻記号層，イベント層，異音層，融合層，母音中心 }
        sasx では最初のブロックである音韻記号層のみを読みます。
        書き出すと音韻記号層のみになります。

 type=3 SR形式
    line-foramt:
        "path start(ms) end(ms) id1 id2 m_start(ms) m_end(ms) utterance"
    path:
        コマンドラインで指定した波形ファイルの path と完全一致した窓にのみ
        そのラベルを表示します。
        type=0(Auto) の場合、path が数字で始まると type=2(ATR) と判断
        してしまいます。
    start end:
        目的の区間の開始時刻と終了時刻を %f 形式で ms で指定します。
        従って、周波数を微妙に間違えた場合(11.025kHzを11kHzとする等)、
        ファイルの後ろのほうでずれてしまいます。
    id1 id2:
        整数です。昔のATRのテスト用ラベルの名残で、特に何に使うかは決まって
        いません。単語番号などの補助情報を入れています。
        このフィールドは編集できません。読んだまま書くだけです。
    m_start:
        発声区間の前にマージンを最大限つけた場合の開始時刻を示します。
        普通はファイルの先頭 0 ですが、切り出されていないファイルの途中を
        指定する使い方では、前の発話の最後を指定します。
    m_end:
        発声区間の後にマージンを最大限つけた時の終了時刻を示します。
        普通はファイルの長さですが、切り出されていないファイルの途中を
        指定する使い方では、次の発話の先頭を指定します。
    utterance:
        なければ勝手に数字(#1,#2,...)をつけます。
    備考:
        この形式は私達が音声認識用に使っているラベルです。

 type=0 自動判定
    判定ルール {
        第１フィールドが数字なら           -->  ATR ラベルとします
        第１フィールドが数字以外なら {
            第４～第７フィールドが数字なら --> SR ラベルとします
            それ以外なら   --> SEG ラベルとします
        }
    }


＜ラベル編集モードについて＞

  mode=0 ラベルの数が可変の場合
    発話内容が不明で、内容に合わせてラベルを生成する場合
    区間を削除したら、削除区間と一緒にラベル文字列も削除される
    区間を挿入したら、空の文字列が割当たる

  mode=1 ラベルの数が固定の場合
    発話内容が既知だが、どの区間に対応しているかを調整する場合
    区間を削除したら、ラベル文字列は後ろの区間にシフトされる
    区間を挿入したら、後ろの区間のラベル文字列が割当たる

=============================================================================
変更履歴など
=============================================================================
＜2.1での変更点＞
  ・ピッチ抽出を更に見直して、サイン波的なものや、下限40Hzくらいまで
    対応できるようになりました。
  ・ATRラベルのイベント層以後を無視するようにしました。
    (ver1.6まではそうなっていたのですが)
  ・スペクトログラムの周波数目盛が、窓のサイズの具合によって、細かすぎる事が
    あったのを修正
  ・複数ピッチをとれるような実験コードを入れました。
    メニューでpitchNumを調整してください
  ・メニュー構成を少し替えました。FFToption をトップから選べるようになりました

＜2.0 から 2.1 への変更点＞
  ・WAV ヘッダと AU ヘッダを解釈するようになりました。
    -tA(-ta) ではとりあえず WAV,AU を試して、該当すればヘッダに従います。
    なければとりあえず -tS(-ts) と同じルーチンを使います。
    -tA をデフォルトとしました。
    表示後は、propertyメニューで freq, chan, endian, type を変えられます。
    -ts などを指定すると強制的にヘッダ解釈をスキップして従来通り、ヘッダも
    データとして表示します。
    ヘッダを再度解釈させるには、propertyメニューで reset を選んでください。
  ・最初に複数窓を表示する時に、ウィンドウマネージャがタイトルを付けるのを
    待つ時間を10msから50msにしました。たまに遅いマシンで失敗するため。
  ・オフセットを設定できるようになりました。ずれたファイルを表示上
    そろえて見たりするのに使用します。

＜2.0 での変更点＞
  ・2.0.09 でピッチ抽出アルゴリズムを改良しました。
    以前は高域をカットしてから分析していたので高い音が検出できなかったのを、
    高域のゲインを 1/f で小さくするだけにし、分析できるようになりました。
    印象としては、不連続なピッチがそれなりに不連続に出るようになりました。

＜1.7 から 2.0 への変更点＞

  ・大きな変更点としては、ポップアップメニューで各種パラメータが
    変更できるようになりました。( 相変わらず Xlib で書いてます )
    メニューを出すには、
      [波形・スペクトログラム窓では]  中央ボタン(ホイール)
      [分析窓では]  中央ボタンでパラメータ変更、右ボタンで表示グラフの切り替え
    メニュー構成はちゃんと練れていないので、今後変わる可能性大です。
    したがって説明はかきませんので、使いかたは適当に探って下さい。
  ・MFCC のスペクトログラムを追加しました。
    スペクトログラム用の MFCC は、認識に使っているものと異なります。
    ana 窓での MFCC2 と同じ分析方法をとっています。
    フィルタバンクのtiltを補正し、lifter なしで分析した結果を
    256ポイントでフーリエ変換 ( COS変換の近似 ) しています。
    FFTスペクトルとのコンパチビリティーのためです。


＜1.7での変更＞ ～ 2001.10.31

分析窓の描画のスレッド化(Makefile の変更が必要です)
  ・選択領域を大きくするとさすがに動作が遅くなるので、スレッド化して、
    間に合わない時は描換えをサボります。変化のスムーズさはなくなりますが、
    応答はマウスについて来るようになります。
  ・滅多に必要無いので現状スムーズさを優先して無効にしてあります。

対応ファイルの拡張
  ・-ts(short) -tp(plot) に加えて -tl(long) -tf(float) -td(double)のファイルを
    short ファイルと同様に扱えます。ただしサウンド再生できるのは -ts のみです。

スケールの改良
  ・スケールの目盛表示間隔が少し変わりました。
  ・波形,スペクトログラムでの横軸にH:M:Sモードを追加しました。
    s キーで SEC → H:M:S → SAMPLE → FRAME → SEC と変わります。

ラベル編集の改良
  ・-Lmode[=1]とすると、区間の消去や挿入時にラベルの文字列は詰め合わせ
    を行います。標準では詰め合わせないで、区間と一緒に削除、生成されます。
  ・ラベル表示状態において、文字列を選ぶと同時に音声区間が選択されます。
  ・ラベル編集モードで、区間同士のオーバーラップは許しませんでしたが、
    CTRLキーを押しながら境界線を移動させた場合は、オーバーラップを許し
    ます。終端と次の始端が同時刻の場合は、従来通り CTRL を押しながら
    動かした最初の方向によって、始端移動か終端移動かが決定されます。
    右に動かすと始端の移動、左に動かすと終端の移動となります。
  ・長いサンプル(44kHz*2ch*数分など)の途中で再生が止まるバグを修正しました。
  ・-Larc[=1] または r キーで区間を示す補助線を引きます。rはトグル
    隣接区間とオーバーラップがある区間は常に引きます。
  ・Tab Shift-Tab で次の区間を選んでいくようにした。
  ・'!' で選択したラベル区間を直前のラベル区間とマージします。
    -Lmode=0 の時はラベルの文字列を','で区切って繋ぎます

分析窓の充実など
  ・分析窓で各種分析方法によるスペクトルが比較できます(key 0-5)
       FFTSPECT,LPCSPECT,LPCMELSPECT,LPCCEPSPECT,LPCMELCEPSPECT,MFCCSPECT
  ・分析窓の横軸(周波数)は s キーで／リニア／Ｍｅｌ／対数／を切り替えられます
  ・分析窓では 左drag で表示領域を移動し、CTRL-左drag でスケールを伸縮できます
    縦、横のスケールの上で始めると縦または横だけの移動、伸縮ができます
    左クリックするとその付近を中心に 1.4 倍に拡大します
    右クリックで、元のスケールに戻ります
  ・波形窓の左端で 左drag すると縦スケールを移動できます
  ・波形/スペクトログラム窓の左端で CTRL-左drag すると縦スケールを伸縮できます
  ・分析窓で w を押して single じゃないモードを選ぶと、選んだ範囲の長さのまま
    ではなく、分析幅,分析周期で適当な回数平均して表示します。平均は対数スペク
    トルの世界で行います。
  ・マウスドラッグによる表示スケールの変更はリアルタイムに行いますが、長い
    区間を表示／分析している時は非常に重くなるので注意して下さい。
    スレッド化するべきですが、現在困っていないのでそのままにしています。
  ・b-tool のアップデートにより、'5' キーで mfccspect を呼び出します
  ・分析窓で 'e' を押すと分析区間のパワーを表示／非表示します。
    プリエンファシス+窓かけ後の１サンプルあたりの平均エネルギー(dB)です
    プリエンファシスを外すには p を押す. 窓を外すには h を押す
  ・分析窓で 'p' を押すと、プリエンファシス自体を一時的にスキップします
    -Apre=xxx の係数の指定は残ります。再度 'p' を押すと元の値に戻ります。

  ・その他細かなデバッグ

＜1.6から1.7への変更点＞ 2001.04.06

スペクトル分析窓の追加：
  ・波形窓またはスペクトログラム窓で数字の0を押すと分析窓が開きます。
  ・分析窓は幾つでも開きます。
  ・スペクトル分析窓上のカーソルは同期されます。
  ・分析窓上でFFT,LPCのモードを切り替えできます。LPCとFFTの同時表示もできます
  ・波形窓上のカーソル位置を中心として -n で指定したフレームサイズの
    波形を分析して表示します。カーソルの動きにリアルタイムに同期します。
  ・分析の窓は-n -m で指定した長さとスキップとで分析する平均モードと、
    選択した範囲を１個の窓として分析するsingleモードがあります。
    現在 w キーで切り替えるようになっています。

  ・全ての波形窓上の選択範囲を分析して表示します。選択位置の変更は即座に
    反映されます。1.7b では毎回全ファイルを分析しなおします。この処理は
    重いはずですが、最近のＰＣは早いのであまり気になりませんが、
    遅いＰＣでは重いかもしれません。


＜1.5から1.6への変更箇所＞ 2000.07.05

プロット画面の追加：
  ・xgraph 形式のテキスト入力を、波形とみなして表示します。
  ・入力テキスト形式は２カラムです。それぞれ X, Y 座標を表し、
    X 軸の値はサンプル番号、Y 軸の値は波形のサンプル値です。
  ・X, Y の値は float です(int でも構いません)。
  ・空行が入るとそこから別の折れ線を開始します。
  ・"text のようにダブルクオートで始まる行があると、textを
    凡例に表示します。
  ・X軸がサンプル番号に対応しますので、波形ファイルと同期を
    とる時便利です。

＜1.4から1.5への変更箇所＞

ラベル編集機能の追加：
  ・ATR 形式のラベルの第１層だけ編集します。編集後のファイルは
    １層めだけになります。
  ・入力が認識(SR)用のラベルファイルの場合も、それなりに
    編集します。多数ファイルを含むリストファイルの場合、表示さ
    せた波形ファイルに関する部分だけ修正して、リスト全体をセー
    ブします。
  ・sasxのウィンドウ上で、ラベル表示エリアの中でのマウスボタン
    操作はラベルの編集操作として扱います。波形やスペクトル上で
    のマウス操作は従来の操作と同じです。
  ・マウスで境界線のラベル表示エリア部分(下の方)を左クリックす
    ると編集対象となり、境界線の上下に■を表示します。
    (draw形式のグラフィックエディタみたいに)
  ・編集対象の(■付きの)境界は、左ボタンドラッグで移動可能です。
  ・SHIFT を押しながら編集対象の境界をドラッグすると、動かした
    方向のセグメントを分割した上で、その分割境界を移動すること
    ができます。境界の左右どちら側のセグメントを分割するかは、
    ドラッグして動かす最初の方向に依存します。
    例えば i | u の境界を SHIFT+ドラッグする場合、i の方向に動
    かせば i を分割し、 u  の方向に動かせば u を分割します。
  ・CTRLを押しながら境界を動かすと、直前または直後のセグメント
    と密着していたものを分離し、間に空白を入れる事ができます。
    (ＡＴＲ形式のラベルではこの使い方はないかもしれません)
  ・-Lsave="filename" で指定したファイルに随時編集結果を書き込
    みます。指定が無ければ結果は残りません。
  ・ラベルの文字列をクリックするとリバース表示になり、ラベル文
    字列あるいはセグメントそのものが編集対象となります。
  ・ラベルの文字列を編集するには、起動時に -Lstdを指定する必要
    があります。
  ・-Lstd を指定すると、ラベル文字列を標準入力で編集できます。
    編集対象(リバース表示中)のラベルを再度左クリックすると、
    sasx を起動した窓の標準入力から文字列を読み込みます。
    ［注意］標準入力を読み込むためには sasx はフォアグランドで
    動作している必要があります。&を付けて実行しないで下さい。
    (そのうちＸの入力編集窓を開けるようにしたいと思っています)
    (Cut+PasteでEUC日本語も入れられます(ASCIIと混在は無理))
  ・ラベルが編集対象になった状態で DEL又は BS を押すと、そのセ
    グメントを消去します。
  ・^U は Undo ですが、現状は境界の移動しか元に戻せません。
  ・従来の -l で指定したファイルを元にして編集します。
    -Lsave= で指定する名前を元のラベル同じ名前にしない限り、元
    のラベルは変更しません。
  ・全く何もないところからラベルを作る機能はないので、そういう
    場合、少なくとも１個の区間のラベルが必要です。

ピッチ表示の変更：
  ・ピッチ検出しないフレームは0Hzで表示する
    (いずれピッチ編集モードを作るため)

バグ修正：
  ・マルチチャンネルファイルのスペクトルが再びバグっていたので
    修正しました。

初期窓のモード設定
  ・-M mgg とする事で、開いた窓で m g g とキーを押したのと同じ
    結果が最初から表示されます。現状使えるのは {m g i o p} です。

表示移動量のオプション(1.5.2以降)
  ・-V でキー操作による表示位置の移動量を設定します。
    -V90 で、キー f,b での移動量を表示範囲の 90% に設定します。
    -V90,20 で、キー f,b を 90% F,B を 20% に設定します。
    デフォルトは 100,10 です

＜1.3から1.4への変更箇所＞

複数窓の同期がサンプル数から時刻になったので異なるサンプリングの
ファイルを見る時に楽になりました。

広帯域と狭帯域のスペクトログラムのフレームサイズとシフトをmsで
別々に指定するようにしました。サンプリング周波数が違っても
デフォルトでだいたい見やすいようになっています。
デフォルトは、
	-W5,2	 広帯域スペクトル；窓幅   5ms、スキップ 2ms
	-N100,10 狭帯域スペクトル；窓幅 100ms、スキップ 10ms

また、スペクトログラムの描き方を修正したので、少し早く、少し
見やすくなりました。拡大して細かく見る場合は -W5,0.5 くらいに
すると、昔ATRプログラムで印刷したイメージに近くなります。

スケールやマーカ（ラベル）の色を指定できるようにしたので、
ハードコピーを重視する時は全部 black にするなどできます。
マーカの線を黒にする場合、
	-Cmk=black （ または -Cmk=#000000 ）
とします。


