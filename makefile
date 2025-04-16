# Makefile İçeriği

CC = cc                      # Derleyici olarak cc kullanıyoruz
CFLAGS = -lws2_32             # Windows için gerekli kütüphane
SOURCES = main.c router.c handlers.c utils.c  # Derlenecek kaynak dosyalar
OUTPUT = server.exe           # Çıktı dosyası

# Varsayılan hedef (all), output dosyasını oluşturur
all: $(OUTPUT)

# Çıktı dosyasını oluşturmak için kaynak dosyalarını derle
$(OUTPUT): $(SOURCES)
	$(CC) $(SOURCES) -o $(OUTPUT) $(CFLAGS)

# Temizleme işlemi (derlenen dosyayı siler)
clean:
	rm -f $(OUTPUT)

# .PHONY, "make clean" gibi komutları, dosya adıyla çakışmadıklarından emin olmak için kullanılır
.PHONY: all clean
