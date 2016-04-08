#include <mot.h>
#include "mot/util.h"

#include <iostream>
#include <bitset>

using namespace std;
using namespace std::chrono;

namespace mot
{
    int timepoint_to_mjd(int timepoint)
    {
        time_t t = timepoint;
        struct tm *tm;
        tm = gmtime(&t);
        int day = tm->tm_mday;
        int month = tm->tm_mon + 1;
        int year = tm->tm_year + 1900;

    return
        367 * year
        - 7 * (year + (month + 9) / 12) / 4
        - 3 * ((year + (month - 9) / 7) / 100 + 1) / 4
        + 275 * month / 9
        + day
        + 1721028
        - 2400000;
    }

    vector<unsigned char> timepoint_to_encoded_utc(int timepoint)
    {
        if(timepoint == 0) // NOW
        {
            bitset<32> bits(0);
            return bits_to_bytes(bits);
        }

        if(timepoint / (60))
        {
            int mjd = timepoint_to_mjd(timepoint);
            seconds s(timepoint);
            hours hh = duration_cast<hours>(s) % 24;
            minutes mm = duration_cast<minutes>(s % chrono::hours(1));
            seconds ss = duration_cast<seconds>(s % chrono::minutes(1));
            milliseconds msec = duration_cast<milliseconds>(s % chrono::seconds(1));

            // only seconds precision right now
            bitset<16> lower(0 + // millis (10)
                            (ss.count() << 10)); // seconds (6) 
            bitset<32> bits((mm.count()) + // minutes (6)
                           (hh.count() << 6) + // hours (5)
                           (1 << 11) + // UTC flag (1)
                           (0 << 12) + // RFU (2)
                           (mjd << 14) + // MJD (17)
                           (1 << 31)); // validity (1)
            return bits_to_bytes(bits) + bits_to_bytes(lower);
        }
        else
        {
            int mjd = timepoint_to_mjd(timepoint);
            seconds s(timepoint);
            hours hh = duration_cast<hours>(s) % 24;
            minutes mm = duration_cast<minutes>(s % chrono::hours(1));
            bitset<32> bits((mm.count()) + // minutes (6)
                           (hh.count() << 6) + // hours (5)
                           (0 << 11) + // UTC flag (1)
                           (0 << 12) + // RFU (2)
                           (mjd << 14) + // MDJ (17)
                           (1 << 31)); // validity (1)
            return bits_to_bytes(bits);
        }
    }

    MotObject::MotObject(int transportId, ContentName &name, const vector<unsigned char> &body, ContentType type)
        : transportId(transportId), name(name), body(body), type(type)
    {
        addParameter(&name);
    }

    MotObject::MotObject(int transportId, string name, const vector<unsigned char> &body, ContentType type)
        : transportId(transportId), name(ContentName(name)), body(body), type(type)
    {
        addParameter(new ContentName(name));
    }

    MotObject::MotObject(int transportId, const char *name, const vector<unsigned char> &body, ContentType type)
        : transportId(transportId), name(ContentName(name)), body(body), type(type)
    {
        addParameter(new ContentName(string(name)));
    }

    MotObject::MotObject(int transportId, ContentName &name, ContentType type)
        : transportId(transportId), name(name), type(type)
    {
        addParameter(&name);
    }

    MotObject::MotObject(int transportId, string name, ContentType type)
        : transportId(transportId), name(ContentName(name)), type(type)
    {
        addParameter(new ContentName(name));
    }

    MotObject::MotObject(int transportId, const char *name, ContentType type)
        : transportId(transportId), name(ContentName(name)), type(type)
    {
        addParameter(new ContentName(string(name)));
    }

    void MotObject::addParameter(HeaderParameter *parameter)
    {
        parameters.push_back(parameter);
    }

    template <typename T>
    vector<HeaderParameter*> MotObject::getParameterByType()
    {
        vector<HeaderParameter*> result;
        for(HeaderParameter *parameter : parameters)
        {
            if(typeid(parameter) == typeid(T))
            {
                result.push_back(parameter);
            }
        }
        return result;
    }

    vector<unsigned char> MotObject::encodeHeader() const
    {
        vector<unsigned char> data;

        // encode the parameters
        vector<unsigned char> header_data;
        for(HeaderParameter* param : this->getParameters())
        {
            header_data = header_data + param->encode();
        }

        // assemble the core
        bitset<56> core_header_bits(this->getType().subtype + // content subtype (9)
                       (this->getType().type << 9) + // content type (6))
                       ((header_data.size() + 7) << 15) + // header size (13)
                       (this->getBody().size() << 28)); // body size (28)

        data = data + bits_to_bytes(core_header_bits);
        data = data + header_data; 

        return data;	
    }

    template <typename T>
    bool MotObject::hasParameter()
    {
        for(HeaderParameter* p : parameters)
        {
            if(typeid(p) == typeid(T))
            {
                return true;
            }
        }
        return false;
    }
    //
    //void MotObject::removeParameter(HeaderParameter& parameter)
    //{
    //	parameters.erase(find(parameters.begin(), parameters.end(), parameter));
    //};

    Parameter::Parameter(int id)
        : id(id)
    { }

    Parameter::~Parameter()
    { }

    vector<unsigned char> Parameter::encode()
    {
        vector<unsigned char> data;
        vector<unsigned char> encoded_param_data = this->encodeData();

        if(encoded_param_data.size() == 0)
        {
            bitset<8> pli_bits((this->getId()) + // ParamId (6)
                              (0 << 6)); // PLI=0 (2)
            data = data + bits_to_bytes(pli_bits);
        }
        else if(encoded_param_data.size() == 1)
        {
            bitset<8> pli_bits((this->getId()) + // ParamId (6)
                              (1 << 6)); // PLI=1 (2)
            data = data + bits_to_bytes(pli_bits);
        }
        else if(encoded_param_data.size() == 4)
        {
            bitset<8> pli_bits((this->getId()) + // ParamId (6)
                              (2 << 6)); // PLI=2 (2)
            data = data + bits_to_bytes(pli_bits);
        }
        else if(encoded_param_data.size() <= 127)
        {
            bitset<16> pli_bits(encoded_param_data.size() + // DataFieldLength (7)
                               (0 << 7) + // Ext=0
                               (this->getId() << 8) + // ParamId (6)
                               (3 << 14)); // PLI=3 (2)
            data = data + bits_to_bytes(pli_bits);
        }
        else
        {
            bitset<24> pli_bits(encoded_param_data.size() + // DataFieldLength (15)
                               (1 << 15) + // Ext=1
                               (this->getId() << 16) + // ParamId (6)
                               (3 << 22)); // PLI=3 (2)
            data = data + bits_to_bytes(pli_bits);
        }

        data = data + encoded_param_data;
        return data; 
    }



    HeaderParameter::HeaderParameter(int id)
        : Parameter(id)
    { }

    HeaderParameter::~HeaderParameter()
    { }

    ContentName::ContentName(const string &name, Charset charset)
        : HeaderParameter(12), name(name), charset(charset)
    { }

    bool ContentName::equals(const HeaderParameter& other) const
    {
        const ContentName* that = dynamic_cast<const ContentName*>(&other);
        return that != nullptr && (this->charset == that->charset &&
                this->name == that->name);
    }

    vector<unsigned char> ContentName::encodeData() const
    {
        bitset<8> bits(charset << 4); // charset(4)

        vector<unsigned char> bytes = bits_to_bytes(bits);
        copy(name.begin(), name.end(), back_inserter(bytes));

        return bytes;
    }

    MimeType::MimeType(const string &mimetype)
        : HeaderParameter(16), mimetype(mimetype)
    { }

    bool MimeType::equals(const HeaderParameter& other) const
    {
        const MimeType* that = dynamic_cast<const MimeType*>(&other);
        return that != nullptr && (this->mimetype == that->mimetype);
    }

    vector<unsigned char> MimeType::encodeData() const
    {
        vector<unsigned char> bytes;
        copy(mimetype.begin(), mimetype.end(), back_inserter(bytes));
        return bytes;
    }

    RelativeExpiration::RelativeExpiration(long offset)
        : HeaderParameter(9), offset(offset)
    { }

    bool RelativeExpiration::equals(const HeaderParameter& other) const
    {
        const RelativeExpiration* that = dynamic_cast<const RelativeExpiration*>(&other);
        return that != nullptr && (this->offset == that->offset);
    }

    vector<unsigned char> RelativeExpiration::encodeData() const
    {
        if(offset < (127 * 60)) // < 127m
        {
            bitset<8> bits((offset/(2*60)) + // offset in 2m interval (6)
                           (0 << 6)); // granularity (2)
            return bits_to_bytes(bits);
        }
        else if(offset < (1891 * 60)) // < 1891m
        {
            bitset<8> bits((offset/(30*60)) + // offset in 30m interval (6)
                           (1 << 6)); // granularity (2)
            return bits_to_bytes(bits);
        }
        else if(offset < (127 * 60 * 60)) // < 127h
        {
            bitset<8> bits((offset/(2*60*60)) + // offset in 2h interval (6)
                           (2 << 6)); // granularity (2)
            return bits_to_bytes(bits);
        }
        else if(offset < (64 * 24 * 60 * 60)) // < 64d
        {
            bitset<8> bits((offset/(24*60*60)) + // offset in 1d interval (6)
                           (3 << 6)); // granularity (2)
            return bits_to_bytes(bits);
        }

        return vector<unsigned char>(); // TODO raise exception
    }

    AbsoluteExpiration::AbsoluteExpiration(long timepoint)
        : HeaderParameter(9), timepoint(timepoint)
    { }

    bool AbsoluteExpiration::equals(const HeaderParameter& other) const
    {
        const AbsoluteExpiration* that = dynamic_cast<const AbsoluteExpiration*>(&other);
        return that != nullptr && (this->timepoint == that->timepoint);
    }

    vector<unsigned char> AbsoluteExpiration::encodeData() const
    {
        return timepoint_to_encoded_utc(timepoint);
    }

    Compression::Compression(CompressionType type)
        : HeaderParameter(17), type(type)
    { }

    vector<unsigned char> Compression::encodeData() const
    {
        bitset<8> bits(type);
        return bits_to_bytes(bits);
    }

    bool Compression::equals(const HeaderParameter& other) const
    {
        const Compression* that = dynamic_cast<const Compression*>(&other);
        return that != nullptr && (this->type == that->type);
    }

    Priority::Priority(unsigned short int priority)
        : HeaderParameter(10), priority(priority)
    { }

    vector<unsigned char> Priority::encodeData() const
    {
        bitset<8> bits(priority);
        return bits_to_bytes(bits);
    }

    bool Priority::equals(const HeaderParameter& other) const
    {
        const Priority* that = dynamic_cast<const Priority*>(&other);
        return that != nullptr && (this->priority == that->priority);
    }

    DirectoryParameter::DirectoryParameter(int id)
        : Parameter(id)
    { }

    DirectoryParameter::~DirectoryParameter()
    { }

    SortedHeaderInformation::SortedHeaderInformation() :
        DirectoryParameter(0)
    { }

    vector<unsigned char> SortedHeaderInformation::encodeData() const
    {
        return vector<unsigned char>();
    }

    Segment::Segment(int transportId, const vector<unsigned char> &data, int index, int repetition, SegmentDatagroupType type, bool last)
        : transportId(transportId), data(data), index(index), repetition(repetition), type(type), last(last)
    { }

    vector<unsigned char> Segment::encode() const {
        bitset<16> bits(data.size() + // segment size (13)
                       (repetition << 13)); // repetition (3)
        vector<unsigned char> bytes = bits_to_bytes(bits);
        bytes.insert(bytes.end(), data.begin(), data.end());
        return bytes;
    }

    SegmentEncoder::SegmentEncoder(SegmentationStrategy* strategy)
        : strategy(strategy)
    { }

    vector<Segment*> SegmentEncoder::encode(MotObject object)
    {
        vector<Segment*> segments;
        
        int chunk_size = strategy->getSegmentSize(object);

        // segment header data
        vector<unsigned char> header_data = object.encodeHeader();
        vector<vector<unsigned char> > chunked_segments = chunk_segments(header_data, chunk_size);
        for (auto i = chunked_segments.begin(); i != chunked_segments.end(); ++i)
        {
            segments.push_back(
                    new Segment(object.getTransportId(), *i, distance(chunked_segments.begin(), i), 0, SegmentDatagroupTypes::Header,
                            (i == (chunked_segments.end() - 1)) ? true : false));
        }

        // segment body data
        vector<unsigned char> body_data = object.getBody();
        chunked_segments = chunk_segments(body_data, chunk_size);
        for(auto i = chunked_segments.begin(); i != chunked_segments.end(); ++i)
        {
            segments.push_back(
                    new Segment(object.getTransportId(), *i, distance(chunked_segments.begin(), i), 0, SegmentDatagroupTypes::Body,
                            (i == (chunked_segments.end() - 1)) ? true : false));
        }

        return segments;
    }

    vector<Segment*> SegmentEncoder::encode(int transportId, vector<MotObject> objects)
    {
        vector<DirectoryParameter*> parameters;
        return this->encode(transportId, objects, parameters);
    }    

    vector<Segment*> SegmentEncoder::encode(int transportId, vector<MotObject> objects, vector<DirectoryParameter*> parameters)
    {
        vector<Segment*> segments;

        // encode the directory
        vector<unsigned char> directory_data;

        // directory entries
        vector<unsigned char> directory_entries_data;
        for(MotObject object : objects)
        {
            directory_entries_data = directory_entries_data + bits_to_bytes(bitset<16>(object.getTransportId())) + object.encodeHeader();
        }

        // directory extension
        vector<unsigned char> directory_extension_data;
        for(DirectoryParameter* parameter : parameters)
        {
            directory_extension_data = directory_extension_data + parameter->encode();
        }
        directory_extension_data = bits_to_bytes(bitset<16>(directory_extension_data.size())) + directory_extension_data;

        // calculate directory object size
        int directory_size = 4 + 2 + 5 + directory_extension_data.size() + directory_entries_data.size();

        // put it all together
        directory_data = directory_data + bits_to_bytes(bitset<32>(directory_size));
        directory_data = directory_data + bits_to_bytes(bitset<16>(objects.size()));
        directory_data = directory_data + bits_to_bytes(bitset<40>()); // 24 bits datacarouselperiod + 16 bits segment size
        directory_data = directory_data + directory_extension_data;
        directory_data = directory_data + directory_entries_data;

        // segment the directory
        vector<vector<unsigned char> > chunked_segments = chunk_segments(directory_data, strategy->getSegmentSize());
        for(auto i = chunked_segments.begin(); i != chunked_segments.end(); ++i)
        {
            segments.push_back(
                    new Segment(transportId, *i, distance(chunked_segments.begin(), i), 0, SegmentDatagroupTypes::Directory_Uncompressed,
                            (distance(i, (chunked_segments.end() - 1)) == 0) ? true : false));
        }

        // segment up the objects
        for(MotObject object : objects)
        {
            // segment the data
            vector<vector<unsigned char> > chunked_segments = chunk_segments(object.getBody(), strategy->getSegmentSize(object));
            for(auto i = chunked_segments.begin(); i != chunked_segments.end(); ++i)
            {
                segments.push_back(new Segment(object.getTransportId(), *i, distance(chunked_segments.begin(), i), 0, SegmentDatagroupTypes::Body,
                            (distance(i, --chunked_segments.end()) == 0) ? true : false));
            }
        }

        return segments;
    }

    vector<vector<unsigned char> > SegmentEncoder::chunk_segments(vector<unsigned char> data, int chunk_size)
    {
        vector<vector<unsigned char> > chunks;

        vector<unsigned char>::iterator start, end;
        start = end = data.begin();
        while(end != data.end())
        {
            int b = distance(end, data.end());
            int step = min(chunk_size, b);
            advance(end, step);
            vector<unsigned char> chunk(end - start);
            copy(start, end, chunk.begin());
            advance(start, step);
            chunks.push_back(chunk);
        }

        return chunks;
    }

    ConstantSizeSegmentationStrategy::ConstantSizeSegmentationStrategy(int size)
        : size(size)
    { }

    int ConstantSizeSegmentationStrategy::getSegmentSize() {
        return size;
    }

    int ConstantSizeSegmentationStrategy::getSegmentSize(const MotObject &object) {
        return size;
    }

    int SequentialTransportIdGenerator::next()
    {
        return ++last;
    }

    SequentialTransportIdGenerator* SequentialTransportIdGenerator::instance;

    SequentialTransportIdGenerator::SequentialTransportIdGenerator(int initial) 
        : last(initial-1) 
    {}


    int RandomTransportIdGenerator::next()
    {
        int next = rand() % (1<<16) + 1;
        while(find(ids.begin(), ids.end(), next) != ids.end())
        {
            int next = rand() % (1<<16) + 1;
        }
        ids.push_back(next);
        return next;
    }


    RandomTransportIdGenerator* RandomTransportIdGenerator::instance;
}
