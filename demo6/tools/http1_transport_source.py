"""Concrete ordinary-language transport methods shared by TCP and TLS.

Static source expansion avoids a runtime transport discriminator and keeps
async waits in the existing Future continuation scheduler.
"""
from pathlib import Path

def sources(root):
    template=(root/'src/stdlib/http1_transport.xl.in').read_text()
    result={'xl6StdlibHttpLimitsSource':(root/'src/stdlib/http1_limits.xl').read_text()}
    for wire,stream in [('Tcp','TcpStream'),('Tls','TlsStream')]:
        name='Http1'+wire+'Transport';methods=[]
        for asynchronous in [False,True]:
            suffix='Async' if asynchronous else '';prefix='async ' if asynchronous else ''
            wait='await ' if asynchronous else ''
            recv='recvOwned'+suffix+'Until'
            plain='deadline, maximum' if wire=='Tcp' else 'maximum, deadline'
            cancelled='deadline, token, maximum' if wire=='Tcp' else 'maximum, deadline, token'
            methods.append(f'''
    private {prefix}func fill{suffix}(uint64 deadline, CancellationToken? cancel) as bool
        if self.ended then return false end
        int maximum = self.bufferLimit - self.pending.length
        if maximum <= 0 then throw RangeError "HTTP/1 input buffer limit reached" end
        if maximum > self.receiveSize then maximum = self.receiveSize end
        auto token = cancel
        if token != null then
            auto chunk = {wait}self.connection.{recv}Cancel({cancelled})
            if chunk.length == 0 then
                self.ended = true
                return false
            end
            self.pending.appendRef(chunk)
        else
            auto chunk = {wait}self.connection.{recv}({plain})
            if chunk.length == 0 then
                self.ended = true
                return false
            end
            self.pending.appendRef(chunk)
        end
        return true
    end

    private {prefix}func head{suffix}(bool response, string method, uint64 deadline, CancellationToken? cancel) as Http1Head?
        auto token = cancel
        if token != null then token.throwIfCancelled() end
        self.enter()
        int next = 5
        try
            while true
                Http1Head? head = null
                if response then
                    head = Http1Head.responseFromNetBuffer(self.pending, self.headLimit, self.startLimit, self.lineLimit, self.fieldLimit)
                else
                    head = Http1Head.requestFromNetBuffer(self.pending, self.headLimit, self.startLimit, self.lineLimit, self.fieldLimit)
                end
                if head != null then
                    auto mode = head.bodyMode(method)
                    Http1BodyReader? body = null
                    if mode != HttpBodyMode.none and mode != HttpBodyMode.tunnel then
                        if mode != HttpBodyMode.fixed or head.contentLength != uint64(0) then
                            body = Http1BodyReader(head, method, self.bodyLimit, self.chunkLimit, self.trailerLimit, self.trailerLineLimit, self.trailerFieldLimit)
                        end
                    end
                    self.pending.consume(head.headerBytes)
                    self.activeBody = body
                    self.finalTrailers = null
                    self.hasMessage = true
                    next = 1
                    if mode == HttpBodyMode.tunnel then
                        next = 2
                        self.activeBody = null
                    elseif body == null then
                        next = 0
                        self.activeBody = null
                    end
                    return head
                end
                if self.ended then
                    if self.pending.length != 0 then throw IOError "truncated HTTP/1 head" end
                    next = 4
                    return null
                end
                {wait}self.fill{suffix}(deadline, cancel)
            end
        finally
            self.phase.store(next)
        end
    end

    {prefix}func readRequestHead{suffix}(int timeoutMillis = -1, CancellationToken? cancel = null) as Http1Head?
        return {wait}self.head{suffix}(false, "", Http1.deadline(timeoutMillis), cancel)
    end
    {prefix}func readResponseHead{suffix}(string requestMethod = "GET", int timeoutMillis = -1, CancellationToken? cancel = null) as Http1Head?
        return {wait}self.head{suffix}(true, requestMethod, Http1.deadline(timeoutMillis), cancel)
    end

    private {prefix}func body{suffix}(Buffer? output, bool whole, uint64 deadline, CancellationToken? cancel) as HttpBodyStatus
        auto token = cancel
        if token != null then token.throwIfCancelled() end
        int old = self.enter(true)
        int next = 5
        try
            if old == 0 then
                next = 0
                if not self.hasMessage then throw RuntimeError "read an HTTP/1 head first" end
                return HttpBodyStatus.done
            end
            auto reader = self.activeBody
            if reader == null then throw RuntimeError "missing HTTP/1 body reader" end
            while true
                HttpBodyStatus status = HttpBodyStatus.more
                if output == null then
                    status = reader.discardFromNetBuffer(self.pending, self.ended)
                else
                    status = reader.readFromNetBuffer(self.pending, output, self.ended)
                end
                if reader.done then
                    if reader.mode == HttpBodyMode.chunked then self.finalTrailers = reader.trailers() end
                    self.activeBody = null
                    next = 0
                    return status
                end
                if status == HttpBodyStatus.data and not whole then
                    next = 1
                    return status
                end
                if status == HttpBodyStatus.more and reader.consumed == 0 then
                    if self.ended then throw IOError "truncated HTTP/1 body" end
                    {wait}self.fill{suffix}(deadline, cancel)
                end
            end
        finally
            self.phase.store(next)
        end
    end

    {prefix}func readBodyInto{suffix}(Buffer output, int timeoutMillis = -1, CancellationToken? cancel = null) as HttpBodyStatus
        return {wait}self.body{suffix}(output, false, Http1.deadline(timeoutMillis), cancel)
    end
    {prefix}func readBodyAllInto{suffix}(Buffer output, int timeoutMillis = -1, CancellationToken? cancel = null) as int
        int before = output.length
        {wait}self.body{suffix}(output, true, Http1.deadline(timeoutMillis), cancel)
        return output.length - before
    end
    {prefix}func discardBody{suffix}(int timeoutMillis = -1, CancellationToken? cancel = null)
        {wait}self.body{suffix}(null, true, Http1.deadline(timeoutMillis), cancel)
    end
''')
        result['xl6StdlibHttp'+wire+'TransportSource']=template.replace('@NAME@',name).replace('@STREAM@',stream).replace('@IO_METHODS@','\n'.join(methods))
    return result
