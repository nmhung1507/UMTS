*Bộ thư viện modem UMTS.
*Yêu cầu: FreeRTOS, LWIP.
*Gồm có các module:
- GPS: gồm gps.c và gps.h: tầng application xử lý việc nhận GPS.
- MUX: gồm các file: GsmMux.c GsmMux.h và GsmMux_Cfg.h. Handle việc tạo mux, truyền nhận
dữ liệu theo mux.
- PPPOS: Xử lý kết nối PPP, Các hàm liên quan đến GSM.
- umts.c và umts.h: tầng xử lý AT command. Dùng chung cho các chip UMTS như Ublox, quectel...
- serial.c và serial.h: tầng thấp hơn, chuyên về truyền nhận Uart.
- umts_porting.c và porting.h: tầng thấp nhất, gọi các hàm truyền nhận low level.
  Khi dùng các MCU khác nhau cần config lại các hàm low level vào file này.
- umts_config.h: config chip, config các hằng số, tùy vào project.

*Cách sử dụng:
- Config các hàm low level vào file umts_porting.h:
  + Hàm UMTS_OpenMainUART_LL: bật ngắt nhận main uart.
  + UMTS_OpenGpsUART_LL: Bật ngắt nhận uart gps (trong trường hợp không dùng MUX).
  + Hàm RxCpltCallback: xử lý khi nhận dữ liệu xong(nhận từng byte một).
    Check nếu là main uart thì gửi vào gMainUartQueue bằng hàm xQueueSendToBackFromISR.
	nếu là gps uart thì gửi vào queue gGpsUartQueue.
  + Hàm TxCpltCallback: xử lý khi gửi dữ liệu xong. trong hàm cần set biến bTransmitting = false.
  + UMTS_Transmit_LL: Hàm transmit uart low level.
- Trong file umts_config.h config dùng mux hay không qua hằng: UMTS_USE_MUX.
- Init các UART cần thiết và gọi hàm Serial_Init().
- Init mux bằng hàm Mux_Init() nếu UMTS_USE_MUX = 1.
- Gọi hàm GPS_Init(), get RMC data bằng hàm GPS_GetData();
- Đối với GSM: Gọi hàm tcpip_init(NULL, NULL) và PPPOS_Init(true). Nếu connect thành công sẽ
lấy được IP, gateway và netmask.
	