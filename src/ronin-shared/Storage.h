/***
 * Demonstrike Core
 */

#pragma once

// Storage max based off guid entry max
static unsigned long STORAGE_ARRAY_MAX = 0x00FFFFFF;

// Previously we used a hash/unordered map, but we'll test standard mapping
#if 1 == 1
#define STORAGE_MAP RONIN_MAP
#else
#define STORAGE_MAP RONIN_UNORDERED_MAP
#endif

/** Base iterator class, returned by MakeIterator() functions.
 */
template<class T>
class SERVER_DECL StorageContainerIterator
{
protected:
    /** Currently referenced object
     */
    T * Pointer;
public:
    virtual ~StorageContainerIterator() {}

    /** Returns the currently stored object
     */
    RONIN_INLINE T * Get() { return Pointer; }

    /** Sets the current object to P
     */
    RONIN_INLINE void Set(T * P) { Pointer = P; }

    /** Are we at the end of the storage container?
     */
    RONIN_INLINE bool AtEnd() { return (Pointer == 0); }

    /** Virtual function to increment to the next element
     */
    virtual bool Inc() = 0;

    /** Virtual function to destroy the iterator
     */
    virtual void Destruct() = 0;
};

template<class T>
class SERVER_DECL ArrayStorageContainer
{
public:
    /** This is where the magic happens :P
     */
    T ** _array;

    /** Maximum possible entry
     */
    uint32 _max;

    /** Returns an iterator currently referencing the start of the container
     */
    StorageContainerIterator<T> * MakeIterator();

    /** Do we need to get the max?
    */
    bool NeedsMax()
    {
        return true;
    }

    /** Creates the array with specified maximum
     */
    void Setup(uint32 Max)
    {
        _array = new T*[Max];
        _max = Max;
        memset(_array, 0, sizeof(T*) * Max);
    }

    /** Sets up the array with a different maximum
     */
    void Resetup(uint32 Max)
    {
        if(Max < _max)
            return;     // no need to realloc

        T ** a = new T*[Max];
        memset(a,0,sizeof(T*)*Max);
        memcpy(a, _array, sizeof(T*) * _max);
        delete [] _array;
        _array = a;
        _max = Max;
    }

    /** Frees the container array and all elements inside it
     */
    ~ArrayStorageContainer()
    {
        for(uint32 i = 0; i < _max; ++i)
            if(_array[i] != NULL)
                delete _array[i];
        delete [] _array;
    }

    /** Allocates entry Entry in the array and sets the pointer, and returns
     * the allocated memory.
     */
    T * AllocateEntry(uint32 Entry)
    {
        if(Entry >= _max || _array[Entry] != 0)
            return reinterpret_cast<T*>(0);

        _array[Entry] = new T();
        return _array[Entry];
    }

    /** Deallocates the entry Entry in the array and sets the pointer to null.
     */
    bool DeallocateEntry(uint32 Entry)
    {
        if(Entry >= _max || _array[Entry] == NULL)
            return false;

        delete _array[Entry];
        _array[Entry] = NULL;
        return true;
    }

    /** Looks up entry Entry and returns the pointer if it is existant, otherwise null.
     */
    T * LookupEntry(uint32 Entry)
    {
        if(Entry >= _max)
            return reinterpret_cast<T*>(NULL);
        else
            return _array[Entry];
    }

    /** Sets the pointer to entry Entry to Pointer, and if it already exists frees the existing
     * element.
     */
    bool SetEntry(uint32 Entry, T * Pointer)
    {
        if(Entry > _max)
            return false;
        if(_array[Entry] != NULL)
            delete _array[Entry];
        _array[Entry] = Pointer;
        return true;
    }

    /** Returns the current pointer if it exists, otherwise allocates it.
     */
    T * LookupEntryAllocate(uint32 Entry)
    {
        if(T * ret = LookupEntry(Entry))
            return ret;
        return AllocateEntry(Entry);
    }

    /** Deletes all entries in the container.
     */
    void Clear()
    {
        for(uint32 i = 0; i < _max; ++i)
        {
            if(_array[i] != 0)
                delete _array[i];
            _array[i] = 0;
        }
    }
};

template<class T> class SERVER_DECL HashMapStorageContainer
{
public:
    typename STORAGE_MAP<uint32, T*> _map;

    /** Returns an iterator currently referencing the start of the container
     */
    StorageContainerIterator<T> * MakeIterator();

    /** Frees the container array and all elements inside it
     */
    ~HashMapStorageContainer()
    {
        for(typename STORAGE_MAP<uint32, T*>::iterator itr = _map.begin(); itr != _map.end(); ++itr)
            delete itr->second;
    }

    /** Do we need to get the max?
     */
    bool NeedsMax()
    {
        return false;
    }

    /** Creates the array with specified maximum
     */
    void Setup(uint32 Max)
    {

    }

    void Resetup(uint32 Max)
    {

    }

    /** Allocates entry Entry in the array and sets the pointer, and returns
     * the allocated memory.
     */
    T * AllocateEntry(uint32 Entry)
    {
        if(_map.find(Entry) != _map.end())
            return reinterpret_cast<T*>(0);
        T * n = new T();
        memset(n, NULL, sizeof(T*));
        _map.insert( std::make_pair( Entry, n ) );
        return n;
    }

    /** Deallocates the entry Entry in the array and sets the pointer to null.
     */
    bool DeallocateEntry(uint32 Entry)
    {
        typename STORAGE_MAP<uint32, T*>::iterator itr = _map.find(Entry);
        if(itr == _map.end())
            return false;

        delete itr->second;
        _map.erase(itr);
        return true;
    }


    T * LookupEntry(uint32 Entry)
    {
        typename STORAGE_MAP<uint32, T*>::iterator itr = _map.find(Entry);
        if(itr == _map.end())
            return reinterpret_cast<T*>(0);
        return itr->second;
    }

    /** Sets the pointer to entry Entry to Pointer, and if it already exists frees the existing
     * element.
     */
    bool SetEntry(uint32 Entry, T * Pointer)
    {
        typename STORAGE_MAP<uint32, T*>::iterator itr = _map.find(Entry);
        if(itr == _map.end())
        {
            _map.insert( std::make_pair( Entry, Pointer ) );
            return true;
        }

        delete itr->second;
        itr->second = Pointer;
        return true;
    }

    /** Returns the current pointer if it exists, otherwise allocates it.
     */
    T * LookupEntryAllocate(uint32 Entry)
    {
        if(T * ret = LookupEntry(Entry))
            return ret;
        return AllocateEntry(Entry);
    }

    /** Deletes all entries in the container.
     */
    void Clear()
    {
        typename STORAGE_MAP<uint32, T*>::iterator itr = _map.begin();
        for(; itr != _map.end(); ++itr)
            delete itr->second;
        _map.clear();
    }
};

template<class T>
class SERVER_DECL ArrayStorageIterator : public StorageContainerIterator<T>
{
    ArrayStorageContainer<T> * Source;
    uint32 MyIndex;
public:

    /** Increments the iterator
    */
    bool Inc()
    {
        GetNextElement();
        if(StorageContainerIterator<T>::Pointer != 0)
            return true;
        return false;
    }

    /** Frees the memory occupied by this iterator
    */
    void Destruct()
    {
        delete this;
    }

    /** Constructor
    */
    ArrayStorageIterator(ArrayStorageContainer<T> * S) : StorageContainerIterator<T>(), Source(S), MyIndex(0)
    {
        GetNextElement();
    }

    /** Sets the next element pointer, or to 0 if we reached the end
    */
    void GetNextElement()
    {
        while(MyIndex < Source->_max)
        {
            if(Source->_array[MyIndex] != 0)
            {
                StorageContainerIterator<T>::Set(Source->_array[MyIndex]);
                ++MyIndex;
                return;
            }
            ++MyIndex;
        }
        // reached the end of the array
        StorageContainerIterator<T>::Set(reinterpret_cast<T*>(0));
    }
};

template<class T>
class SERVER_DECL HashMapStorageIterator : public StorageContainerIterator<T>
{
    HashMapStorageContainer<T> * Source;
    typename STORAGE_MAP<uint32, T*>::iterator itr;
public:

    /** Constructor
    */
    HashMapStorageIterator(HashMapStorageContainer<T> * S) : StorageContainerIterator<T>(), Source(S)
    {
        itr = S->_map.begin();
        if(itr == S->_map.end())
            StorageContainerIterator<T>::Set(0);
        else StorageContainerIterator<T>::Set(itr->second);
    }

    /** Gets the next element, or if we reached the end sets it to 0
    */
    void GetNextElement()
    {
        ++itr;
        if(itr == Source->_map.end())
            StorageContainerIterator<T>::Set(0);
        else StorageContainerIterator<T>::Set(itr->second);
    }

    /** Returns true if we're not at the end, otherwise false.
    */
    bool Inc()
    {
        GetNextElement();
        if(StorageContainerIterator<T>::Pointer != 0)
            return true;
        return false;
    }

    /** Frees the memory occupied by this iterator
    */
    void Destruct()
    {
        delete this;
    }
};

#ifndef SCRIPTLIB
template<class T>
StorageContainerIterator<T> * ArrayStorageContainer<T>::MakeIterator()
{
    return new ArrayStorageIterator<T>(this);
}

template<class T>
StorageContainerIterator<T> * HashMapStorageContainer<T>::MakeIterator()
{
    return new HashMapStorageIterator<T>(this);
}
#endif

template<class T, class StorageType>
class SERVER_DECL Storage
{
protected:
    StorageType _storage;
    char * _indexName;
    char * _formatString;
public:

    RONIN_INLINE char * GetIndexName() { return _indexName; }
    RONIN_INLINE char * GetFormatString() { return _formatString; }

    /** False constructor to fool compiler
     */
    Storage() {}
    virtual ~Storage() {}

    /** Makes an iterator, w00t!
     */
    StorageContainerIterator<T> * MakeIterator()
    {
        return _storage.MakeIterator();
    }

    /** Calls the storage container lookup function.
     */
    T * LookupEntry(uint32 Entry)
    {
        return _storage.LookupEntry(Entry);
    }

    /** Reloads the content in this container.
     */
    virtual void Reload() = 0;

    /** Loads the container using the specified name and format string
     */
    virtual void Load(const char * IndexName, const char * FormatString)
    {
        _indexName = strdup(IndexName);
        _formatString = strdup(FormatString);
    }

    /** Frees the duplicated strings and all entries inside the storage container
     */
    virtual void Cleanup()
    {
        StorageContainerIterator<T> * itr = _storage.MakeIterator();
        while(!itr->AtEnd())
        {
            FreeBlock(itr->Get());
            if(!itr->Inc())
                break;
        }
        itr->Destruct();

        _storage.Clear();
        free(_indexName);
        free(_formatString);
    }

    /** Frees any string elements inside blocks.
     */
    void FreeBlock(T * Allocated)
    {
        char * p = _formatString;
        char * structpointer = (char*)Allocated;
        for(; *p != 0; ++p)
        {
            switch(*p)
            {
            case 's':       // string is the only one we have to actually do anything for here
                free((*(char**)structpointer));
                structpointer += sizeof(char*);
                break;
            case 'u':
            case 'i':
            case 'f':
                structpointer += sizeof(uint32);
                break;

            case 'h':
                structpointer += sizeof(uint16);
                break;

            case 'c':
                structpointer += sizeof(uint8);
                break;
            }
        }
    }
};

template<class T, class StorageType>
class SERVER_DECL SQLStorage : public Storage<T, StorageType>
{
public:
    SQLStorage() : Storage<T, StorageType>() {}
    ~SQLStorage() {}

    /** Creates a new block.
     */
    T* CreateBlock(uint32 entry)
    {
        return Storage<T, StorageType>::_storage.AllocateEntry(entry);
    }

    /** Sets up a new block.
     */
    void SetBlock(uint32 entry, T* p)
    {
        Storage<T, StorageType>::_storage.SetEntry(entry, p);
    }

    /** Loads the block using the format string.
     */
    RONIN_INLINE void LoadBlock(Field * fields, T * Allocated, bool reload = false )
    {
        char * p = Storage<T, StorageType>::_formatString;
        char * structpointer = (char*)Allocated;
        uint32 offset = 0;
        Field * f = fields;
        for(; *p != 0; ++p, ++f)
        {
            switch(*p)
            {
            case 'b':   // Boolean
                {
                    *(bool*)&structpointer[offset] = f->GetBool();
                    offset += sizeof(bool);
                }break;

            case 'c':   // Char
                {
                    *(uint8*)&structpointer[offset] = f->GetUInt8();
                    offset += sizeof(uint8);
                }break;

            case 'h':   // Short
                {
                    *(uint16*)&structpointer[offset] = f->GetUInt16();
                    offset += sizeof(uint16);
                }break;

            case 'u':   // Unsigned integer
                {
                    *(uint32*)&structpointer[offset] = f->GetUInt32();
                    offset += sizeof(uint32);
                }break;

            case 'i':   // Signed integer
                {
                    *(int32*)&structpointer[offset] = f->GetInt32();
                    offset += sizeof(int32);
                }break;

            case 'f':   // Float
                {
                    *(float*)&structpointer[offset] = f->GetFloat();
                    offset += sizeof(float);
                }break;

            case 's':   // Null-terminated string
                {
                    const char* str = f->GetString();
                    if(str == NULL)
                        *(char**)&structpointer[offset] = "";
                    else
                        *(char**)&structpointer[offset] = strdup(str);
                    offset += sizeof(char*);
                }break;

            case 'x':   // Skip
                break;

            default:    // unknown
                printf("Unknown field type in string: `%c`\n", *p);
                break;
            }
        }
    }

    void Load(std::string IndexName, const char * FormatString)
    {
        Load(IndexName.c_str(), FormatString);
    }

    /** Loads from the table.
     */
    void Load(const char * IndexName, const char * FormatString)
    {
        Storage<T, StorageType>::Load(IndexName, FormatString);
        QueryResult * result;
        if(Storage<T, StorageType>::_storage.NeedsMax())
        {
            result = WorldDatabase.Query("SELECT MAX(entry) FROM %s ORDER BY `entry`", IndexName);
            uint32 Max = STORAGE_ARRAY_MAX;
            if(result)
            {
                Max = result->Fetch()[0].GetUInt32() + 1;
                if(Max > STORAGE_ARRAY_MAX)
                {
                    sLog.Warning("Storage", "The table, '%s', has been limited to maximum of %u entries.\
                        Any entry higher than %u will be discarded.",
                        IndexName, STORAGE_ARRAY_MAX, Max );

                    Max = STORAGE_ARRAY_MAX;
                }
                delete result;
            }

            Storage<T, StorageType>::_storage.Setup(Max);
        }

        size_t cols = strlen(FormatString);
        result = WorldDatabase.Query("SELECT * FROM %s", IndexName);
        if (!result)
            return;
        Field * fields = result->Fetch();

        if(result->GetFieldCount() != cols)
        {
            if(result->GetFieldCount() > cols)
            {
                sLog.Warning("Storage", "Invalid format in %s (%u/%u), loading anyway because we have enough data\n", IndexName, (unsigned int)result->GetFieldCount(), (unsigned int)cols);
            }
            else
            {
                sLog.Error("Storage", "Invalid format in %s (%u/%u), not enough data to proceed.\n", IndexName, (unsigned int)result->GetFieldCount(), (unsigned int)cols);
                delete result;
                return;
            }
        }

        uint32 Entry;
        T * Allocated;
        do
        {
            Entry = fields[0].GetUInt32();
            Allocated = Storage<T, StorageType>::_storage.AllocateEntry(Entry);
            if(!Allocated)
                continue;

            LoadBlock(fields, Allocated);
        } while(result->NextRow());
        sLog.Notice("Storage", "%u entries loaded from table %s.", result->GetRowCount(), IndexName);
        delete result;

        //sLog.Success("Storage", "Loaded database cache from `%s`.", IndexName);
    }

    /** Loads from the worldmapinfo table.
    Crow: Instead of deleting data, we can just skip it.
    Usable for other tables, with a few changes to their structure. */
    void LoadWithLoadColumn(const char * IndexName, const char * FormatString)
    {
        Storage<T, StorageType>::Load(IndexName, FormatString);
        QueryResult * result;
        if(Storage<T, StorageType>::_storage.NeedsMax())
        {
            result = WorldDatabase.Query("SELECT MAX(entry) FROM %s ORDER BY `entry`", IndexName);
            uint32 Max = STORAGE_ARRAY_MAX;
            if(result)
            {
                Max = result->Fetch()[0].GetUInt32() + 1;
                if(Max > STORAGE_ARRAY_MAX)
                {
                    sLog.Warning("Storage", "The table, '%s', has been limited to maximum of %u entries.\
                        Any entry higher than %u will be discarded.",
                        IndexName, STORAGE_ARRAY_MAX, Max );

                    Max = STORAGE_ARRAY_MAX;
                }
                delete result;
            }

            Storage<T, StorageType>::_storage.Setup(Max);
        }

        size_t cols = strlen(FormatString);
        result = WorldDatabase.Query("SELECT * FROM %s WHERE `load` = '1'", IndexName);
        if (!result)
            return;

        Field * fields = result->Fetch();
        if(result->GetFieldCount() != cols)
        {
            if(result->GetFieldCount() > cols)
            {
                sLog.Warning("Storage", "Invalid format in %s (%u/%u), loading anyway because we have enough data\n", IndexName, (unsigned int)result->GetFieldCount(), (unsigned int)cols);
            }
            else
            {
                sLog.Error("Storage", "Invalid format in %s (%u/%u), not enough data to proceed.\n", IndexName, (unsigned int)result->GetFieldCount(), (unsigned int)cols);
                delete result;
                return;
            }
        }

        uint32 Entry;
        T * Allocated;
        do
        {
            Entry = fields[0].GetUInt32();
            Allocated = Storage<T, StorageType>::_storage.AllocateEntry(Entry);
            if(!Allocated)
                continue;

            LoadBlock(fields, Allocated);
        } while(result->NextRow());
        sLog.Notice("Storage", "%u entries loaded from table %s.", result->GetRowCount(), IndexName);
        delete result;
    }

    void LoadAdditionalData(const char * IndexName, const char * FormatString)
    {
        Storage<T, StorageType>::Load(IndexName, FormatString);
        QueryResult * result;
        if(Storage<T, StorageType>::_storage.NeedsMax())
        {
            result = WorldDatabase.Query("SELECT MAX(entry) FROM %s", IndexName);
            uint32 Max = STORAGE_ARRAY_MAX;
            if(result)
            {
                Max = result->Fetch()[0].GetUInt32() + 1;
                if(Max > STORAGE_ARRAY_MAX)
                {
                    sLog.Error("Storage", "The table, '%s', has been limited to maximum of %u entries. Any entry higher than %u will be discarted.",
                        IndexName, STORAGE_ARRAY_MAX, Max );

                    Max = STORAGE_ARRAY_MAX;
                }
                delete result;
            }

            Storage<T, StorageType>::_storage.Resetup(Max);
        }

        size_t cols = strlen(FormatString);
        result = WorldDatabase.Query("SELECT * FROM %s", IndexName);
        if (!result)
            return;
        Field * fields = result->Fetch();

        if(result->GetFieldCount() != cols)
        {
            if(result->GetFieldCount() > cols)
            {
                sLog.Error("Storage", "Invalid format in %s (%u/%u), loading anyway because we have enough data\n", IndexName, (unsigned int)result->GetFieldCount(), (unsigned int)cols);
            }
            else
            {
                sLog.Error("Storage", "Invalid format in %s (%u/%u), not enough data to proceed.\n", IndexName, (unsigned int)result->GetFieldCount(), (unsigned int)cols);
                delete result;
                return;
            }
        }

        uint32 Entry;
        T * Allocated;
        do
        {
            Entry = fields[0].GetUInt32();
            Allocated = Storage<T, StorageType>::_storage.LookupEntryAllocate(Entry);
            if(!Allocated)
                continue;

            LoadBlock(fields, Allocated);
        } while(result->NextRow());
        sLog.Notice("Storage", "%u entries loaded from table %s.", result->GetRowCount(), IndexName);
        delete result;
    }

    /** Reloads the storage container
     */
    void Reload()
    {
        sLog.Notice("Storage", "Reloading database cache from `%s`...\n", Storage<T, StorageType>::_indexName);
        QueryResult * result = WorldDatabase.Query("SELECT MAX(entry) FROM %s", Storage<T, StorageType>::_indexName);
        if(result == 0)
            return;

        uint32 Max = result->Fetch()[0].GetUInt32();
        delete result;
        if(!Max)
            return;

        if(Storage<T, StorageType>::_storage.NeedsMax())
        {
            if(Max > STORAGE_ARRAY_MAX)
                Max = STORAGE_ARRAY_MAX;

            Storage<T, StorageType>::_storage.Resetup(Max+1);
        }

        size_t cols = strlen(Storage<T, StorageType>::_formatString);
        result = WorldDatabase.Query("SELECT * FROM %s", Storage<T, StorageType>::_indexName);
        if (!result)
            return;
        Field * fields = result->Fetch();

        if(result->GetFieldCount() != cols)
        {
            sLog.Error("Storage", "Invalid format in %s (%u/%u).", Storage<T, StorageType>::_indexName, (unsigned int)cols, (unsigned int)result->GetFieldCount());
            delete result;
            return;
        }

        uint32 Entry;
        T * Allocated;
        do
        {
            Entry = fields[0].GetUInt32();
            Allocated = Storage<T, StorageType>::_storage.LookupEntryAllocate(Entry);
            if(Allocated)
                LoadBlock(fields, Allocated, true);

        } while(result->NextRow());
        delete result;
    }
};

#undef STORAGE_MAP