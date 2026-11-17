# MiniHTTPd

Servidor HTTP/1.1 básico en C para Linux, sin bibliotecas externas.
Usa `epoll` para atender varios clientes en un solo hilo, con conexiones persistentes (keep-alive).

## Compilar y ejecutar

```bash
make
./minihttpd            # puerto 8080, carpeta www/
./minihttpd 9000 www   # puerto y carpeta personalizados
```

Abrir `http://localhost:8080` en el navegador. `Ctrl+C` detiene el servidor.

## Estructura

| Archivo | Función |
|---|---|
| `src/main.c` | Bucle de eventos con `epoll` |
| `src/server.c` | Sockets: `socket`, `bind`, `listen`, `accept`, `recv`, `send` |
| `src/http.c` | Parsing de la petición GET y generación de la respuesta |
| `src/files.c` | Acceso seguro a los archivos estáticos (`realpath`) |
| `src/mime.c` | Tipos MIME según la extensión |
| `www/` | Archivos servidos (`index.html`, `style.css`, `image.png`) |

## Características

- Solo método `GET`; cualquier otro método responde `405 Method Not Allowed`.
- Cabeceras usadas: `Host` (obligatoria en HTTP/1.1), `Connection`, `User-Agent` (aparece en el log).
- Códigos de estado: 200, 400, 403, 404, 405, 500.
- Tipos MIME: `.html`, `.css`, `.js`, `.png`, `.jpg`/`.jpeg`.

## Seguridad

| Ataque | Defensa |
|---|---|
| Directory traversal (`/../../etc/passwd`) | Decodificar `%XX`, rechazar `..` y comprobar con `realpath()` que la ruta quede dentro de `www/` → 403 |
| Buffer overflow | Sin `strcpy`/`sprintf`: se usan `memcpy` con límite y `snprintf` |
| Métodos inválidos | Todo lo que no sea `GET` → 405 |
| Peticiones grandes | Línea > 2048, URI > 255, cabeceras > 8 KB o > 50 cabeceras → 400 |

## Pruebas rápidas

```bash
curl -i http://localhost:8080/
curl -i --path-as-is http://localhost:8080/../../etc/passwd   # 403
curl -i -X POST http://localhost:8080/                        # 405
curl -i http://localhost:8080/noexiste                        # 404
```
