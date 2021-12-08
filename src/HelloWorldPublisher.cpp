// Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file HelloWorldPublisher.cpp
 *
 */

#include "HelloWorldPubSubTypes.h"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>

using namespace eprosima::fastdds::dds;

class HelloWorldPublisher
{
private:

    HelloWorld hello_;

    DomainParticipant* participant_;

    Publisher* publisher_;

    Topic* topic_;

    DataWriter* writer_;

    TypeSupport type_;

    size_t data_size = 1024;

    class PubListener : public DataWriterListener
    {
    public:

        PubListener()
            : matched_(0)
        {
        }

        ~PubListener() override
        {
        }

        void on_publication_matched(
                DataWriter*,
                const PublicationMatchedStatus& info) override
        {
            if (info.current_count_change == 1)
            {
                matched_ = info.total_count;
                std::cout << "Publisher matched." << std::endl;
            }
            else if (info.current_count_change == -1)
            {
                matched_ = info.total_count;
                std::cout << "Publisher unmatched." << std::endl;
            }
            else
            {
                std::cout << info.current_count_change
                        << " is not a valid value for PublicationMatchedStatus current count change." << std::endl;
            }
        }

        std::atomic_int matched_;

    } listener_;

public:

    HelloWorldPublisher()
        : participant_(nullptr)
        , publisher_(nullptr)
        , topic_(nullptr)
        , writer_(nullptr)
        , type_(new HelloWorldPubSubType())
    {
    }

    virtual ~HelloWorldPublisher()
    {
        if (writer_ != nullptr)
        {
            publisher_->delete_datawriter(writer_);
        }
        if (publisher_ != nullptr)
        {
            participant_->delete_publisher(publisher_);
        }
        if (topic_ != nullptr)
        {
            participant_->delete_topic(topic_);
        }
        DomainParticipantFactory::get_instance()->delete_participant(participant_);
    }

    //!Initialize the publisher
    bool init()
    {
        size_t size = data_size;
        hello_.index(0);
        hello_.message("HelloWorld");
        std::vector<uint32_t> data;
        data.reserve(size / 4);
        for (size_t i = 0; i < size; i += 4)
        {
            data.push_back(0xdeadbeef);
        }
        hello_.data(data);
        std::cout << "send " << std::hex;
        for (uint32_t i : hello_.data())
            std::cout << i << " ";
        std::cout << std::endl;

        DomainParticipantQos participantQos;
        participantQos.name("Participant_publisher");
        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);

        if (participant_ == nullptr)
        {
            return false;
        }

        // Register the Type
        type_.register_type(participant_);

        // Create the publications Topic
        topic_ = participant_->create_topic("HelloWorldTopic", "HelloWorld", TOPIC_QOS_DEFAULT);

        if (topic_ == nullptr)
        {
            return false;
        }

        // Create the Publisher
        publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT, nullptr);

        if (publisher_ == nullptr)
        {
            return false;
        }

        // Create the DataWriter
        std::cout << "Creating reliability for datawriter: ..." << std::endl;
        eprosima::fastdds::dds::ReliabilityQosPolicy reliability_qos;
        reliability_qos.kind = eprosima::fastdds::dds::BEST_EFFORT_RELIABILITY_QOS;
        eprosima::fastdds::dds::DataWriterQos writer_qos;
        eprosima::fastdds::dds::HistoryQosPolicy history_qos;
        history_qos.kind = eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS;
        writer_qos.reliability(reliability_qos);
        writer_qos.history(history_qos);

        std::cout << "Creating datawriter: ..." << std::endl;
        writer_ = publisher_->create_datawriter(topic_, writer_qos, &listener_);

        if (writer_ == nullptr)
        {
            std::cout << "Datawriter not created" << std::endl;
            return false;
        }
        std::cout << "Datawriter created." << std::endl;
        return true;
    }

    //!Send a publication
    bool publish()
    {
        if (listener_.matched_ > 0)
        {
            hello_.index(hello_.index() + 1);
            eprosima::fastrtps::rtps::InstanceHandle_t instance_handle;
            eprosima::fastdds::dds::TypeSupport::ReturnCode_t ret_write =
                writer_->write(&hello_, instance_handle);
            if (
                    ret_write ==
                    eprosima::fastdds::dds::TypeSupport::ReturnCode_t::RETCODE_OK)
            {
                std::cout << "write succeded: ok" << std::endl;
            }
            else if (
                    ret_write ==
                    eprosima::fastdds::dds::TypeSupport::ReturnCode_t::
                    RETCODE_PRECONDITION_NOT_MET)
            {
                std::cout << "write failed: precondition not met" << std::endl;
            }
            else
            {
                std::cout << "write failed: other " << ret_write() << std::endl;
            }
            /*  Alternatively this block could be used to show that the issue also exists using other write implemetations
                if (writer_->write(&hello_, instance_handle))
                {
                    std::cout << "publication succeeded" << std::endl;
                }
                else
                {
                    std::cout << "publication failed" << std::endl;
                }
            */
            return true;
        }
        return false;
    }

    //!Run the Publisher
    void run(
            uint32_t samples)
    {
        uint32_t samples_sent = 0;
        while (samples_sent < samples)
        {
            if (publish())
            {
                samples_sent++;
                std::cout << "Message: " << hello_.message() << " with index: " << hello_.index()
                            << " SENT" << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    void set_size(uint32_t size)
    {
        data_size = size;
    }
};

int main(
        int argc,
        char** argv)
{
    std::cout << "Starting publisher." << std::endl;
    int samples = 10;

    HelloWorldPublisher* mypub = new HelloWorldPublisher();
    if(argc == 2)
    {
        mypub->set_size(atoi(argv[1]));
    }
    if(mypub->init())
    {
        mypub->run(static_cast<uint32_t>(samples));
    }

    delete mypub;
    return 0;
}
