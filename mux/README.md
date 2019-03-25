# Thư viện MUX cho Quectel UC20

## *Tính năng:*  
- Cấu hình UC20 sang chế độ MUX, tạo ra 4 kênh data (DLC) trên cùng 1 đường UART - có thể sử dụng đồng thời 4 DLC cho các tác vụ khác nhau: xử lý AT command, kết nối mạng và truyền nhận dữ liệu (PPP), nhận dữ liệu GPS, ...  
- Implement theo chuẩn GSM 07.10 (một số tính năng về control trên DLC0 chưa implement)  

## *Cấu hình:*  
 - Cấu hình trong file *GsmMux_Cfg.h*  
	 - Cấu hình debug  
	 - Cấu hình hàm sleep  
	 - Cấu hình buffer size nhận UART 
	 - Cấu hình UART Handle để truyền nhận  
	 - Cấu hình statistic  
 - Cấu hình trong file *GsmMux_Port.c*
	 - Implement các hàm *Mux_UARTInit* (enable ngắt nhận dữ liệu UART), *Mux_UARTTransmit* (truyền dữ liệu),  *Mux_OnRxISR* (hàm này cần được đặt trong ngắt của UART để push dữ liệu nhận được vào queue để thư viện xử lý) 

## *Sử dụng:*
- Gọi hàm *Mux_Init*: Hàm này sẽ tạo đưa module UC20 vào chế độ mux, khởi tạo 5 DLC (DLC0 cho điều khiển, DLC1-4 để truyền nhận dữ liệu), tạo task ... Dữ liệu từ UC20 được đóng thành các frame và gửi chung trên 1 đường truyền UART. Thư viện nhận dữ liệu này vào một buffer, sau đó bóc tách thành các frame. Nếu là các frame dữ liệu trên các DLC1-4 sẽ gọi các hàm callback tương ứng để xử lý.  
- Gọi hàm *Mux_DLCSetRecvDataCallback* để set hàm callback nhận dữ liệu trên từng kênh DLC (1-4). Các hàm callback này không được block và nên xử lý nhanh nhất có thể để giảm ảnh hưởng đến luồng nhận dữ liệu.  
- Để gửi dữ liệu trên các DLC gọi hàm *Mux_DLCSendData*  
