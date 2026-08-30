================================================================
[Specification] 2-Player Atari Controller to USB & Serial Converter
================================================================

1. Overview
----------------------------------------------------------------
This device reads inputs from two Atari-style controllers and 
outputs them via two methods:
- USB HID: Controller 1 data is sent to a PC as a Joystick.
- Serial Output: Controller 1 & 2 data are sent every 12ms via USART2.

Target Hardware: STM32F103C6T6 (CubeIDE / HAL based)
Communication: UART 115200bps (Recommended) / USB HID Joystick

2. Pin Assignment (GPIO Configuration)
----------------------------------------------------------------
* All inputs: Internal Pull-up, "ON" when connected to GND.

[ Controller 1 (USB / Serial ID:0) ]
- Up           : GPIOB - PIN 9
- Down         : GPIOB - PIN 8
- Left         : GPIOB - PIN 7
- Right        : GPIOB - PIN 6
- Button 1     : GPIOB - PIN 5
- Button 2     : GPIOB - PIN 4
- Button 3     : GPIOB - PIN 3
- Turbo SW 1   : GPIOA - PIN 9  (for Btn1)
- Turbo SW 2   : GPIOA - PIN 10 (for Btn2)
- Turbo SW 3   : GPIOA - PIN 15 (for Btn3)

[ Controller 2 (Serial ID:1) ]
- Up           : GPIOB - PIN 11
- Down         : GPIOB - PIN 10
- Left         : GPIOB - PIN 1
- Right        : GPIOB - PIN 0
- Button 1     : GPIOA - PIN 7
- Button 2     : GPIOA - PIN 6
- Button 3     : GPIOA - PIN 5
- Turbo SW 1   : GPIOA - PIN 0  (for Btn1)
- Turbo SW 2   : GPIOA - PIN 1  (for Btn2)
- Turbo SW 3   : GPIOA - PIN 4  (for Btn3)

[ Communication Ports ]
- USB D+ / D-  : GPIOA - PIN 12 / PIN 11
- USART2 TX    : GPIOA - PIN 2 (Output to external device)
- USART2 RX    : GPIOA - PIN 3 (Not used)

3. Serial Data Protocol
----------------------------------------------------------------
Interval: 12ms (approx. 83.3Hz)
Format: 2-byte binary (Byte 1: P1 Data, Byte 2: P2 Data)

Bit structure for each byte (Active Low logic for MSX/Atari):
bit 7 : ID (0=Controller 1, 1=Controller 2)
bit 6 : Button 3 (★0=ON, 1=OFF)
bit 5 : Button 2 (★0=ON, 1=OFF)
bit 4 : Button 1 (★0=ON, 1=OFF)
bit 3 : Right    (★0=ON, 1=OFF)
bit 2 : Left     (★0=ON, 1=OFF)
bit 1 : Down     (★0=ON, 1=OFF)
bit 0 : Up       (★0=ON, 1=OFF)

4. Rapid-Fire (Turbo) Specification
----------------------------------------------------------------
- Speed: Approx. 12.5 shots/sec (40ms toggle interval)
- Operation: When the Turbo SW pin is connected to GND, the 
  corresponding button will pulse automatically while held down.
================================================================================================================================

Japanese
================================================================
【設計書】2台対応アタリ仕様コントローラー USB＆シリアル変換器
================================================================

1. 概要
----------------------------------------------------------------
本装置は、アタリ仕様コントローラー2台の入力を読み取り、以下の2系統で出力する。
・USB HID出力：コントローラー①の情報のみをPCへ送信。
・シリアル出力：コントローラー①および②の情報を12ms周期で送信。

ターゲット：STM32F103C6T6 (CubeIDE / HAL)
通信設定：UART 115200bps (推奨) / USB HID Joystick

2. ピンアサイン (GPIO構成)
----------------------------------------------------------------
※全入力：内部プルアップ、GND接続時に「ON」

■ コントローラー ① (USB操作 / シリアル ID:0)
・上 (Up)      : GPIOB - PIN 9
・下 (Down)    : GPIOB - PIN 8
・左 (Left)    : GPIOB - PIN 7
・右 (Right)   : GPIOB - PIN 6
・ボタン 1     : GPIOB - PIN 5
・ボタン 2     : GPIOB - PIN 4
・ボタン 3     : GPIOB - PIN 3
・連射SW 1     : GPIOA - PIN 9  (Btn1に対応)
・連射SW 2     : GPIOA - PIN 10 (Btn2に対応)
・連射SW 3     : GPIOA - PIN 15 (Btn3に対応)

■ コントローラー ② (シリアル ID:1)
・上 (Up)      : GPIOB - PIN 11
・下 (Down)    : GPIOB - PIN 10
・左 (Left)    : GPIOB - PIN 1
・右 (Right)   : GPIOB - PIN 0
・ボタン 1     : GPIOA - PIN 7
・ボタン 2     : GPIOA - PIN 6
・ボタン 3     : GPIOA - PIN 5
・連射SW 1     : GPIOA - PIN 0  (Btn1に対応)
・連射SW 2     : GPIOA - PIN 1  (Btn2に対応)
・連射SW 3     : GPIOA - PIN 4  (Btn3に対応)

■ 通信ポート
・USB D+ / D-  : GPIOA - PIN 12 / PIN 11
・USART2 TX    : GPIOA - PIN 2 (外部機器への送信)
・USART2 RX    : GPIOA - PIN 3 (未使用)

3. 通信・連射仕様
----------------------------------------------------------------
【シリアル送信】
・周期：12ms (約83.3Hz) / 受信側60Hzポーリングに最適化
・形式：2バイトバイナリ (1Pデータ -> 2Pデータ の順)
・ビット構造：
  b7   : ID (0=Controller 1, 1=Controller 2)
  b6-b0: 各ボタン・方向 (★0 = ON / 1 = OFF : Active Low)

  [Bit Map]
  b7:ID, b6:Btn3, b5:Btn2, b4:Btn1, b3:右, b2:左, b1:下, b0:上

【連射機能】
・速度：約12.5連射/秒 (40ms反転周期)
・動作：連射SWピンがGND(Low)の時、対応ボタン押しっぱなしで連射作動。
================================================================