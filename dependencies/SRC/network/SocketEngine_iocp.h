/*
 * Sandshroud Project Ronin
 * Copyright (C) 2005-2008 Ascent Team <http://www.ascentemu.com/>
 * Copyright (C) 2008-2009 AspireDev <http://www.aspiredev.org/>
 * Copyright (C) 2009-2017 Sandshroud <https://github.com/Sandshroud>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#ifdef NETLIB_IOCP

struct Overlapped
{
    int m_op;
    void * m_acceptBuffer;
    OVERLAPPED m_ov;
};

class  iocpEngine : public SocketEngine
{
    /** Our completion port
     */
    HANDLE m_completionPort;

    /** Socket set lock
     */
    Mutex m_socketLock;

    /** Socket set
     */
    std::set<BaseSocket*> m_sockets;

    /** Spawned thread count
     */
    int thread_count;

public:
    iocpEngine(int Tlimit);
    ~iocpEngine();

    /** Adds a socket to the engine.
     */
    void AddSocket(BaseSocket * s);

    /** Removes a socket from the engine. It should not receive any more events.
     */
    void RemoveSocket(BaseSocket * s);

    /** This is called when a socket has data to write for the first time.
     */
    void WantWrite(BaseSocket * s);

    /** Spawn however many worker threads this engine requires
     */
    void SpawnThreads();

    /** Called by SocketWorkerThread, this is the network loop.
     */
    void MessageLoop();

    /** Shutdown the socket engine, disconnect any associated sockets and 
     * deletes itself and the socket deleter.
     */
    void Shutdown();
};

enum SocketEvents
{
    IO_EVENT_ACCEPT,
    IO_EVENT_READ,
    IO_EVENT_WRITE,
    IO_SHUTDOWN,
};

inline void CreateSocketEngine(int count) { new iocpEngine(count); }

#endif      // NETLIB_IOCP
