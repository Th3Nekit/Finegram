// Релей обхода для Finegram: MTProto поверх WebSocket.
//
// Принимает WebSocket-соединение от клиента и сращивает его с дата-центром
// Telegram. Никакого адреса от клиента не принимается: единственный выбор — номер
// дата-центра из неизменяемой таблицы, поэтому процесс физически не может
// подключиться никуда, кроме Telegram.
//
// Байты полезной нагрузки непрозрачны (обфусцированный MTProto) — мы их не
// разбираем, не читаем и не пишем в журнал, только переносим.
//
// Пропусков нет: пускаем всех, но скорость каждого ограничена, чтобы один
// человек не съел канал целиком.
//
// Основано на релее WS-Bypass.
package main

import (
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

// dcMap — единственный список адресов, куда релей вообще может подключиться.
// dc=203 — это псевдоним второго дата-центра в сети доставки, он обязателен.
var dcMap = map[int]string{
	1:   "149.154.175.50",
	2:   "149.154.167.51",
	3:   "149.154.175.100",
	4:   "149.154.167.91",
	5:   "149.154.171.5",
	203: "91.105.192.100",
}

const (
	listenAddr = "127.0.0.1:8096"

	dialTimeout   = 5 * time.Second
	idleTimeout   = 4 * time.Minute
	writeTimeout  = 30 * time.Second
	handshakeWait = 10 * time.Second

	// Сколько соединений разрешено одному человеку. Клиенту нужно по одному на
	// дата-центр плюс запас на переподключения; всё сверх — либо ошибка, либо
	// злоупотребление.
	maxConnsPerIP = 32

	// Через сколько без единого соединения забываем про адрес.
	limiterIdle = 30 * time.Minute
)

// Потолок скорости на человека, байт в секунду. Меняется переменной окружения
// RELAY_BPS; по умолчанию 20 Мбит/с.
var limitBPS float64 = 20_000_000 / 8

// ---------------------------------------------------------------------------
// Ограничение скорости
// ---------------------------------------------------------------------------

// limiter — ведро с токенами на один адрес. Оба направления берут из одного
// ведра: «20 Мбит/с на человека» — это про весь его трафик, а не про каждую
// сторону отдельно.
type limiter struct {
	mu    sync.Mutex
	next  time.Time
	used  time.Time
	conns int
}

// wait задерживает передачу ровно настолько, чтобы средняя скорость не
// превышала потолок. Сон всегда вне замка: иначе ждали бы и остальные.
func (l *limiter) wait(n int) {
	if l == nil || limitBPS <= 0 || n <= 0 {
		return
	}
	l.mu.Lock()
	now := time.Now()
	if l.next.Before(now) {
		l.next = now
	}
	l.next = l.next.Add(time.Duration(float64(n) / limitBPS * float64(time.Second)))
	l.used = now
	delay := time.Until(l.next)
	l.mu.Unlock()

	if delay > 0 {
		time.Sleep(delay)
	}
}

var (
	limitersMu sync.Mutex
	limiters   = map[string]*limiter{}
)

// acquire выдаёт ведро адреса и занимает одно соединение. Возвращает false,
// если у этого адреса уже слишком много открытых туннелей.
func acquire(ip string) (*limiter, bool) {
	limitersMu.Lock()
	defer limitersMu.Unlock()

	l := limiters[ip]
	if l == nil {
		l = &limiter{used: time.Now()}
		limiters[ip] = l
	}
	l.mu.Lock()
	defer l.mu.Unlock()
	if l.conns >= maxConnsPerIP {
		return nil, false
	}
	l.conns++
	l.used = time.Now()
	return l, true
}

func release(l *limiter) {
	if l == nil {
		return
	}
	l.mu.Lock()
	if l.conns > 0 {
		l.conns--
	}
	l.used = time.Now()
	l.mu.Unlock()
}

// forgetIdle убирает адреса, которые давно не появлялись: иначе таблица растёт
// весь срок жизни процесса.
func forgetIdle() {
	for {
		time.Sleep(5 * time.Minute)
		cutoff := time.Now().Add(-limiterIdle)

		limitersMu.Lock()
		for ip, l := range limiters {
			l.mu.Lock()
			idle := l.conns == 0 && l.used.Before(cutoff)
			l.mu.Unlock()
			if idle {
				delete(limiters, ip)
			}
		}
		limitersMu.Unlock()
	}
}

// clientIP — адрес человека за nginx. Свой заголовок доверяем только потому,
// что снаружи в релей никто не ходит: он слушает петлю.
func clientIP(r *http.Request) string {
	if v := strings.TrimSpace(r.Header.Get("X-Real-IP")); v != "" {
		return v
	}
	if v := strings.TrimSpace(r.Header.Get("X-Forwarded-For")); v != "" {
		if comma := strings.IndexByte(v, ','); comma >= 0 {
			v = v[:comma]
		}
		return strings.TrimSpace(v)
	}
	host, _, err := net.SplitHostPort(r.RemoteAddr)
	if err != nil {
		return r.RemoteAddr
	}
	return host
}

// ---------------------------------------------------------------------------
// Приём соединений
// ---------------------------------------------------------------------------

var upgrader = websocket.Upgrader{
	HandshakeTimeout: handshakeWait,
	ReadBufferSize:   32 * 1024,
	WriteBufferSize:  32 * 1024,
	// Клиент — не браузер, проверять источник незачем.
	CheckOrigin: func(r *http.Request) bool { return true },
}

func handleAPIWS(w http.ResponseWriter, r *http.Request) {
	// Куда идём — только по номеру дата-центра из таблицы. Адреса от клиента
	// не принимаются ни в каком виде.
	dc, err := strconv.Atoi(r.URL.Query().Get("dc"))
	if err != nil {
		http.Error(w, "bad dc", http.StatusBadRequest)
		return
	}
	ip, ok := dcMap[dc]
	if !ok {
		http.Error(w, "unknown dc", http.StatusBadRequest)
		return
	}

	who := clientIP(r)
	lim, allowed := acquire(who)
	if !allowed {
		http.Error(w, "too many connections", http.StatusTooManyRequests)
		return
	}
	defer release(lim)

	// Сначала отвечаем клиенту, потом подключаемся к Telegram: так клиент
	// получает свой ответ сразу, а неудачное подключение просто закроет туннель.
	ws, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		return
	}
	defer ws.Close()

	tcp, err := net.DialTimeout("tcp", net.JoinHostPort(ip, "443"), dialTimeout)
	if err != nil {
		log.Printf("dc=%d подключиться к %s:443 не вышло: %v", dc, ip, err)
		_ = ws.WriteControl(websocket.CloseMessage,
			websocket.FormatCloseMessage(websocket.CloseInternalServerErr, "dial failed"),
			time.Now().Add(time.Second))
		return
	}
	defer tcp.Close()

	if t, ok := tcp.(*net.TCPConn); ok {
		_ = t.SetNoDelay(true)
	}

	splice(ws, tcp, lim)
}

// ---------------------------------------------------------------------------
// Перекачка байтов
// ---------------------------------------------------------------------------

// splice переносит байты в обе стороны, пока одна из сторон не закроется.
//
//	от клиента: кадры WebSocket -> поток TCP
//	к клиенту:  поток TCP -> кадры WebSocket
//
// На запросы «ты жив?» отвечаем сами, чтобы клиентский keepalive не рвал связь.
func splice(ws *websocket.Conn, tcp net.Conn, lim *limiter) {
	// Настоящие кадры MTProto маленькие; ограничение не даёт собеседнику
	// заставить нас выделить сколько угодно памяти на одно сообщение.
	ws.SetReadLimit(256 * 1024)

	ws.SetPingHandler(func(data string) error {
		_ = ws.SetReadDeadline(time.Now().Add(idleTimeout))
		err := ws.WriteControl(websocket.PongMessage, []byte(data),
			time.Now().Add(writeTimeout))
		if err == websocket.ErrCloseSent {
			return nil
		}
		return err
	})
	ws.SetPongHandler(func(string) error {
		return ws.SetReadDeadline(time.Now().Add(idleTimeout))
	})

	done := make(chan struct{}, 2)

	// От клиента в Telegram.
	go func() {
		defer func() { done <- struct{}{} }()
		for {
			_ = ws.SetReadDeadline(time.Now().Add(idleTimeout))
			mt, payload, err := ws.ReadMessage()
			if err != nil {
				return
			}
			if mt != websocket.BinaryMessage && mt != websocket.TextMessage {
				continue
			}
			if len(payload) == 0 {
				continue
			}
			lim.wait(len(payload))
			_ = tcp.SetWriteDeadline(time.Now().Add(writeTimeout))
			if _, err := tcp.Write(payload); err != nil {
				return
			}
		}
	}()

	// Из Telegram клиенту.
	go func() {
		defer func() { done <- struct{}{} }()
		buf := make([]byte, 32*1024)
		for {
			_ = tcp.SetReadDeadline(time.Now().Add(idleTimeout))
			n, err := tcp.Read(buf)
			if n > 0 {
				lim.wait(n)
				_ = ws.SetWriteDeadline(time.Now().Add(writeTimeout))
				if werr := ws.WriteMessage(websocket.BinaryMessage, buf[:n]); werr != nil {
					return
				}
			}
			if err != nil {
				return
			}
		}
	}()

	// Кто закончил первым, тот и закрывает туннель: закрытие обоих сокетов
	// выводит вторую горутину из ожидания.
	<-done
	_ = ws.Close()
	_ = tcp.Close()
	<-done
}

// ---------------------------------------------------------------------------

func main() {
	log.SetFlags(log.LstdFlags | log.LUTC)

	if v := strings.TrimSpace(os.Getenv("RELAY_BPS")); v != "" {
		if f, err := strconv.ParseFloat(v, 64); err == nil && f > 0 {
			limitBPS = f
		}
	}

	go forgetIdle()

	mux := http.NewServeMux()
	mux.HandleFunc("/apiws", handleAPIWS)
	mux.HandleFunc("/healthz", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
		_, _ = io.WriteString(w, "ok\n")
	})

	srv := &http.Server{
		Addr:              listenAddr,
		Handler:           mux,
		ReadHeaderTimeout: handshakeWait,
		// Общего срока на запись нет: туннели живут долго, сроки выставляются
		// на каждое соединение внутри splice.
	}

	log.Printf("релей слушает %s: только Telegram, %d дата-центров, потолок %.0f КБ/с на человека",
		listenAddr, len(dcMap), limitBPS/1024)
	if err := srv.ListenAndServe(); err != nil {
		log.Fatalf("сервер остановился: %v", err)
	}
}
