#include "S3Manager.hpp"

static const size_t BUFFER_SIZE = 512 * 1024 * 1024; // 512MB Buffer 

class MyUnderlyingStream : public Aws::IOStream
{
    public:
        using Base = Aws::IOStream;
        // Provide a customer-controlled streambuf to hold data from the bucket.
        explicit MyUnderlyingStream(std::streambuf* buf)
            : Base(buf)
        {}

        ~MyUnderlyingStream() override = default;
};


S3Manager::S3Manager(const std::string& accessKey, const std::string& secretKey)
    : m_accessKey(accessKey), m_secretKey(secretKey) {}
S3Manager::S3Manager(){}    
int S3Manager::listBuckets() {
        Aws::SDKOptions options;
        Aws::InitAPI(options);

        int result = 0;
        {
            Aws::Client::ClientConfiguration clientConfig;
            clientConfig.region = "us-east-1";
            clientConfig.endpointOverride = "http://localhost:4566";

            Aws::Auth::AWSCredentials creds(m_accessKey, m_secretKey);

            if (creds.IsEmpty()) {
                std::cerr << "Failed authentication" << std::endl;
                result = 1;
            } else {
                Aws::S3::S3Client s3Client(creds, nullptr, clientConfig);
                auto outcome = s3Client.ListBuckets();

                if (!outcome.IsSuccess()) {
                    std::cerr << "Failed with error: " << outcome.GetError() << std::endl;
                    result = 1;
                } else {
                    std::cout << "Found " << outcome.GetResult().GetBuckets().size() << " buckets\n";
                    for (auto &bucket : outcome.GetResult().GetBuckets()) {
                        std::cout << bucket.GetName() << std::endl;
                    }
                }
            }
        }

        Aws::ShutdownAPI(options);
        return result;
    }
int S3Manager::putDbObject(const Aws::String fileName,const Aws::String bucketName)
{
    m_accessKey = "test";
    m_secretKey = "test";
    Aws::SDKOptions options;
    Aws::InitAPI(options);

    int result = 0;
    {
        Aws::Client::ClientConfiguration clientConfig;
        clientConfig.region = "us-east-1";
        clientConfig.endpointOverride = "http://localhost:4566/mybucket";
        clientConfig.connectTimeoutMs = 0;
        clientConfig.requestTimeoutMs = 0;
        Aws::Auth::AWSCredentials creds(m_accessKey, m_secretKey);

        if (creds.IsEmpty()) {
            std::cerr << "Failed authentication" << std::endl;
            result = 1;
        } else {
            Aws::S3::S3Client s3Client(creds, nullptr, clientConfig);
            Aws::S3::Model::PutObjectRequest request;
            
            request.SetBucket(bucketName);
            request.SetKey(fileName);
            std::shared_ptr<Aws::IOStream> inputData =
            Aws::MakeShared<Aws::FStream>("SampleAllocationTag",
                                          fileName.c_str(),
                                          std::ios_base::in | std::ios_base::binary);
            if (!*inputData) {
            std::cerr << "Error unable to read file " << fileName << std::endl;
            return false; }

            request.SetBody(inputData);
            request.SetContentType("text/plain");

            Aws::S3::Model::PutObjectOutcome outcome =   s3Client.PutObject(request);
            if (!outcome.IsSuccess()) {
                    std::cerr << "Error: PutObject: " <<
                        outcome.GetError().GetMessage() << std::endl;
                    }
            else {
                std::cout << "Added object '" << fileName << "' to bucket '"
                        << bucketName << "'.";
            }    return outcome.IsSuccess();
        }
    }

    Aws::ShutdownAPI(options);
    return result;
}
void S3Manager::uploadFileMultipart(const std::string& fileName,const std::string& folderName) {
    // Initialize the AWS SDK for C++
    Aws::SDKOptions options;
    Aws::InitAPI(options);

    Aws::Client::ClientConfiguration clientConfig;
    clientConfig.region = "us-east-1";
    clientConfig.endpointOverride = "http://localhost:4566/mybucket";
    clientConfig.connectTimeoutMs = 0;
    clientConfig.requestTimeoutMs = 0;
    Aws::Auth::AWSCredentials creds(m_accessKey, m_secretKey);
    // Initialize the S3 client
    Aws::S3::S3Client s3Client(creds, nullptr, clientConfig);

    // Specify the multipart upload parameters
    Aws::S3::Model::CreateMultipartUploadRequest createRequest;
    createRequest.SetBucket("mybucket");
    createRequest.SetKey(fileName);

    // Create the multipart upload
    auto createOutcome = s3Client.CreateMultipartUpload(createRequest);
    if (!createOutcome.IsSuccess()) {
        std::cerr << "Failed to create multipart upload: " << createOutcome.GetError().GetMessage() << std::endl;
        Aws::ShutdownAPI(options);
        return;
    }

    // Get the upload ID
    Aws::String uploadId = createOutcome.GetResult().GetUploadId();

    // Open the file to upload
    std::ifstream fileStream(fileName, std::ios::binary);
    if (!fileStream) {
        std::cerr << "Failed to open file: " << fileName << std::endl;
        Aws::ShutdownAPI(options);
        return;
    }

    // Get the file size
    fileStream.seekg(0, std::ios::end);
    long fileSize = fileStream.tellg();
    fileStream.seekg(0, std::ios::beg);

    // Specify the part size (5 MB)
    const long long PART_SIZE = 5 * 1024 * 1024;

    // Calculate the number of parts
    int numParts = (fileSize + PART_SIZE - 1) / PART_SIZE;

    // Initialize variables for tracking uploaded parts
    std::vector<Aws::S3::Model::CompletedPart> completedParts;
    completedParts.reserve(numParts);

    // Upload each part
    for (int partNumber = 1; partNumber <= numParts; ++partNumber) {
        // Calculate part size and offset
        long long partSize = std::min(PART_SIZE, fileSize - (partNumber - 1) * PART_SIZE);
        long long partOffset = (partNumber - 1) * PART_SIZE;

        // Read part data from the file
        std::vector<unsigned char> partData(partSize);
        fileStream.read(reinterpret_cast<char*>(partData.data()), partSize);

        // Upload the part
        Aws::S3::Model::UploadPartRequest uploadPartRequest;
        uploadPartRequest.SetBucket("mybucket");
        uploadPartRequest.SetKey(fileName);
        uploadPartRequest.SetPartNumber(partNumber);
        uploadPartRequest.SetUploadId(uploadId);
        uploadPartRequest.SetContentLength(partSize);
        auto partStream = Aws::MakeShared<Aws::StringStream>("PartData");
        *partStream << Aws::String(reinterpret_cast<const char*>(partData.data()), partSize);
        uploadPartRequest.SetBody(partStream);

        auto uploadPartOutcome = s3Client.UploadPart(uploadPartRequest);
        if (!uploadPartOutcome.IsSuccess()) {
            std::cerr << "Failed to upload part " << partNumber << ": " << uploadPartOutcome.GetError().GetMessage() << std::endl;
            Aws::ShutdownAPI(options);
            return;
        }

        // Add the completed part to the list
        Aws::S3::Model::CompletedPart completedPart;
        completedPart.SetPartNumber(partNumber);
        completedPart.SetETag(uploadPartOutcome.GetResult().GetETag());
        completedParts.push_back(completedPart);
    }

    // Complete the multipart upload
    Aws::S3::Model::CompleteMultipartUploadRequest completeRequest;
    completeRequest.SetBucket("mybucket");
    completeRequest.SetKey(fileName);
    completeRequest.SetUploadId(uploadId);
    //completeRequest.WithParts(completedParts);

    auto completeOutcome = s3Client.CompleteMultipartUpload(completeRequest);
    if (!completeOutcome.IsSuccess()) {
        std::cerr << "Failed to complete multipart upload: " << completeOutcome.GetError().GetMessage();
    }
}
int S3Manager::uploadTransferMan(const std::string& fileName,const std::string& folderName)
{
    m_accessKey = "test";
    m_secretKey = "test";
    const char* BUCKET = "mybucket";
    const std::string KEY = fileName;
    const std::string LOCAL_FILE = folderName + "/" +fileName;
    Aws::String LOCAL_FILE_COPY(LOCAL_FILE);
    LOCAL_FILE_COPY += "_copy";

    Aws::SDKOptions options;
    options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace; //Turn on logging.

    Aws::InitAPI(options);
    {
        Aws::Client::ClientConfiguration clientConfig;
        clientConfig.region = "us-east-1";
        clientConfig.endpointOverride = "http://localhost:4566/mybucket/"+folderName+"/";
        Aws::Auth::AWSCredentials creds(m_accessKey, m_secretKey);
        // snippet-start:[transfer-manager.cpp.transferOnStream.code]
        Aws::S3::S3Client s3Client(creds, nullptr, clientConfig);
        auto s3_client = Aws::MakeShared<Aws::S3::S3Client>("s3Client",creds,nullptr,clientConfig);
        auto executor = Aws::MakeShared<Aws::Utils::Threading::PooledThreadExecutor>("executor", 25);
        Aws::Transfer::TransferManagerConfiguration transfer_config(executor.get());
        transfer_config.s3Client = s3_client;
        // Create buffer to hold data received by the data stream.
        Aws::Utils::Array<unsigned char> buffer(BUFFER_SIZE);

        // The local variable 'streamBuffer' is captured by reference in a lambda.
        // It must persist until all downloading by the 'transfer_manager' is complete.
        Aws::Utils::Stream::PreallocatedStreamBuf streamBuffer(buffer.GetUnderlyingData(), buffer.GetLength());

        auto transfer_manager = Aws::Transfer::TransferManager::Create(transfer_config);

        auto uploadHandle = transfer_manager->UploadFile(LOCAL_FILE, BUCKET, KEY, "text/plain", Aws::Map<Aws::String, Aws::String>());
        uploadHandle->WaitUntilFinished();
        bool success = uploadHandle->GetStatus() == Aws::Transfer::TransferStatus::COMPLETED; 
      
        if (!success)
        {
            auto err = uploadHandle->GetLastError();           
            std::cout << "File upload failed:  "<< err.GetMessage() << std::endl;
        }
        else
        {
            std::cout << "File upload finished." << std::endl;

            // Verify that the upload retrieved the expected amount of data.
            assert(uploadHandle->GetBytesTotalSize() == uploadHandle->GetBytesTransferred());

            // auto downloadHandle = transfer_manager->DownloadFile(BUCKET,
            //     KEY,
            //     [&]() { //Define a lambda expression for the callback method parameter to stream back the data.
            //         return Aws::New<MyUnderlyingStream>("TestTag", &streamBuffer);
            //     });
            // downloadHandle->WaitUntilFinished();// Block calling thread until download is complete.
            // auto downStat = downloadHandle->GetStatus();
            // if (downStat != Transfer::TransferStatus::COMPLETED)
            // {
            //     auto err = downloadHandle->GetLastError();
            //     std::cout << "File download failed:  " << err.GetMessage() << std::endl;
            // }
            // std::cout << "File download to memory finished."  << std::endl;
            // // snippet-end:[transfer-manager.cpp.transferOnStream.code]
             
            
            // // Verify the download retrieved the expected length of data.
            // assert(downloadHandle->GetBytesTotalSize() == downloadHandle->GetBytesTransferred());

            // // Verify that the length of the upload equals the download. 
            // assert(uploadHandle->GetBytesTotalSize() == downloadHandle->GetBytesTotalSize());

            // // Write the buffered data to local file copy.
            // Aws::OFStream storeFile(LOCAL_FILE_COPY.c_str(), Aws::OFStream::out | Aws::OFStream::trunc);
            // storeFile.write((const char*)(buffer.GetUnderlyingData()),
            //                 static_cast<std::streamsize>(downloadHandle->GetBytesTransferred()));
            // storeFile.close();

            // std::cout << "File dumped to local file finished. You can verify the two files' content using md5sum." << std::endl;
            // // Verify the upload file is the same as the downloaded copy. This can be done using 'md5sum' or any other file compare tool.
        }
    }
    Aws::ShutdownAPI(options);
    return 0;
}
int S3Manager::downloadFile(std::string fileName,std::string folderName)
{
    const char* BUCKET = "mybucket";
    const std::string KEY = fileName;
    const std::string LOCAL_FILE = folderName + "/" +fileName;
    Aws::String LOCAL_FILE_COPY(LOCAL_FILE);
    LOCAL_FILE_COPY += "_copy";
    Aws::SDKOptions options;
    options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace; //Turn on logging.
    Aws::InitAPI(options);
    {
        Aws::Client::ClientConfiguration clientConfig;
        clientConfig.region = "us-east-1";
        clientConfig.endpointOverride = "http://localhost:4566/mybucket/"+folderName+"/";
        Aws::Auth::AWSCredentials creds(m_accessKey, m_secretKey);
        // snippet-start:[transfer-manager.cpp.transferOnStream.code]
        Aws::S3::S3Client s3Client(creds, nullptr, clientConfig);
        auto s3_client = Aws::MakeShared<Aws::S3::S3Client>("s3Client",creds,nullptr,clientConfig);
        auto executor = Aws::MakeShared<Aws::Utils::Threading::PooledThreadExecutor>("executor", 25);
        Aws::Transfer::TransferManagerConfiguration transfer_config(executor.get());
        transfer_config.s3Client = s3_client;
        // Create buffer to hold data received by the data stream.
        Aws::Utils::Array<unsigned char> buffer(BUFFER_SIZE);

        // The local variable 'streamBuffer' is captured by reference in a lambda.
        // It must persist until all downloading by the 'transfer_manager' is complete.
        Aws::Utils::Stream::PreallocatedStreamBuf streamBuffer(buffer.GetUnderlyingData(), buffer.GetLength());

        auto transfer_manager = Aws::Transfer::TransferManager::Create(transfer_config);
        auto downloadHandle = transfer_manager->DownloadFile(BUCKET,
                KEY,
                [&]() { //Define a lambda expression for the callback method parameter to stream back the data.
                    return Aws::New<MyUnderlyingStream>("TestTag", &streamBuffer);
                });
            downloadHandle->WaitUntilFinished();// Block calling thread until download is complete.
            auto downStat = downloadHandle->GetStatus();
            if (downStat != Aws::Transfer::TransferStatus::COMPLETED)
            {
                auto err = downloadHandle->GetLastError();
                std::cout << "File download failed:  " << err.GetMessage() << std::endl;
            }
            std::cout << "File download to memory finished."  << std::endl;
            // snippet-end:[transfer-manager.cpp.transferOnStream.code]
             
            
            // Verify the download retrieved the expected length of data.
            assert(downloadHandle->GetBytesTotalSize() == downloadHandle->GetBytesTransferred());

            // Verify that the length of the upload equals the download. 
            //assert(uploadHandle->GetBytesTotalSize() == downloadHandle->GetBytesTotalSize());

            // Write the buffered data to local file copy.
            Aws::OFStream storeFile(LOCAL_FILE_COPY.c_str(), Aws::OFStream::out | Aws::OFStream::trunc);
            storeFile.write((const char*)(buffer.GetUnderlyingData()),
                            static_cast<std::streamsize>(downloadHandle->GetBytesTransferred()));
            storeFile.close();

            std::cout << "File dumped to local file finished. You can verify the two files' content using md5sum." << std::endl;
            // Verify the upload file is the same as the downloaded copy. This can be done using 'md5sum' or any other file compare tool.
    }
    Aws::ShutdownAPI(options);
    return 0;
}
bool S3Manager::CreateBucket(const Aws::String &bucketName) {
    Aws::SDKOptions options;
    options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace; //Turn on logging.

    Aws::InitAPI(options);
    Aws::Client::ClientConfiguration clientConfig;
    clientConfig.region = "us-east-1";    
    clientConfig.endpointOverride = "http://localhost:4566/mybucket/";
    clientConfig.enableEndpointDiscovery = true;
 
    Aws::Auth::AWSCredentials creds(m_accessKey, m_secretKey);
        // snippet-start:[transfer-manager.cpp.transferOnStream.code]
    Aws::S3::S3Client client(creds, nullptr, clientConfig);
    Aws::S3::Model::CreateBucketRequest request;
    request.SetBucket(bucketName);

    //TODO(user): Change the bucket location constraint enum to your target Region.
    if (clientConfig.region != "us-east-1") {
        Aws::S3::Model::CreateBucketConfiguration createBucketConfig;
        createBucketConfig.SetLocationConstraint(
                Aws::S3::Model::BucketLocationConstraintMapper::
                GetBucketLocationConstraintForName(
                        clientConfig.region));
        request.SetCreateBucketConfiguration(createBucketConfig);
    }

    Aws::S3::Model::CreateBucketOutcome outcome = client.CreateBucket(request);
    if (!outcome.IsSuccess()) {
        auto err = outcome.GetError();
        std::cerr << "Error: CreateBucket: " <<
                  err.GetExceptionName() << ": " << err.GetMessage() << std::endl;
    }
    else {
        std::cout << "Created bucket " << bucketName <<
                  " in the specified AWS Region." << std::endl;
    }
    Aws::ShutdownAPI(options);
    return outcome.IsSuccess();
}
bool S3Manager::findFileInBucket(const Aws::String &bucketName, const Aws::String &fileName)
{
    return false;
}
bool S3Manager::searchObjectInBucket(const Aws::String& bucketName, const Aws::String& objectKey)
{
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    {
        m_accessKey = "test";
        m_secretKey = "test";
        // Set up the S3 client
        Aws::Client::ClientConfiguration clientConfig;
        clientConfig.region = "us-east-1";    
        clientConfig.endpointOverride = "http://localhost:4566/mybucket/";
        clientConfig.enableEndpointDiscovery = true;
        Aws::Auth::AWSCredentials creds(m_accessKey, m_secretKey);
        Aws::S3::S3Client client(creds, nullptr, clientConfig);    
        // Set up the request to list objects in the bucket
        Aws::S3::Model::ListObjectsRequest objectsRequest;
        objectsRequest.WithBucket(bucketName);

        // List the objects in the bucket
        auto outcome = client.ListObjects(objectsRequest);

        // Check if the request was successful
        if (outcome.IsSuccess()) {
            const auto& objects = outcome.GetResult().GetContents();
            for (const auto& object : objects) {
                if (object.GetKey() == objectKey) {
                    std::cout << "Object '" << objectKey << "' found in bucket '" << bucketName << "'" << std::endl;
                    return true;
                }
            }
            //std::cout << "Object '" << objectKey << "' not found in bucket '" << bucketName << "'" << std::endl;
            return false;
        } else {
            //std::cerr << "Failed to list objects in bucket '" << bucketName << "': " << outcome.GetError().GetMessage() << std::endl;
            return false;
        }
    }
    Aws::ShutdownAPI(options);
}
std::pair<std::string,std::string> splitPath(const std::string& path)
{
    fs::path p(path);
    return {p.parent_path().string(),p.filename().string()};
}
int S3Manager::downloadS3File(std::string path)
{
    m_accessKey = "test";
    m_secretKey = "test";
    auto [folderName,fileName] = splitPath(path);
  //  std::string fileName;
    //std::string folderName;
    const char* BUCKET = "mybucket";
    const std::string KEY = fileName;
    const std::string LOCAL_FILE = folderName + "/" +fileName;
    Aws::String LOCAL_FILE_COPY(LOCAL_FILE);
    LOCAL_FILE_COPY += "_copy";
    Aws::SDKOptions options;
    options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace; //Turn on logging.
    Aws::InitAPI(options);
    {
        Aws::Client::ClientConfiguration clientConfig;
        clientConfig.region = "us-east-1";
        clientConfig.endpointOverride = "http://localhost:4566/mybucket/"+folderName+"/";
        Aws::Auth::AWSCredentials creds(m_accessKey, m_secretKey);
        // snippet-start:[transfer-manager.cpp.transferOnStream.code]
        Aws::S3::S3Client s3Client(creds, nullptr, clientConfig);
        auto s3_client = Aws::MakeShared<Aws::S3::S3Client>("s3Client",creds,nullptr,clientConfig);
        auto executor = Aws::MakeShared<Aws::Utils::Threading::PooledThreadExecutor>("executor", 25);
        Aws::Transfer::TransferManagerConfiguration transfer_config(executor.get());
        transfer_config.s3Client = s3_client;
        // Create buffer to hold data received by the data stream.
        Aws::Utils::Array<unsigned char> buffer(BUFFER_SIZE);

        // The local variable 'streamBuffer' is captured by reference in a lambda.
        // It must persist until all downloading by the 'transfer_manager' is complete.
        Aws::Utils::Stream::PreallocatedStreamBuf streamBuffer(buffer.GetUnderlyingData(), buffer.GetLength());

        auto transfer_manager = Aws::Transfer::TransferManager::Create(transfer_config);
        auto downloadHandle = transfer_manager->DownloadFile(BUCKET,
                KEY,
                [&]() { //Define a lambda expression for the callback method parameter to stream back the data.
                    return Aws::New<MyUnderlyingStream>("TestTag", &streamBuffer);
                });
            downloadHandle->WaitUntilFinished();// Block calling thread until download is complete.
            auto downStat = downloadHandle->GetStatus();
            if (downStat != Aws::Transfer::TransferStatus::COMPLETED)
            {
                auto err = downloadHandle->GetLastError();
                std::cout << "File download failed:  " << err.GetMessage() << std::endl;
            }
            std::cout << "File download to memory finished."  << std::endl;
            // snippet-end:[transfer-manager.cpp.transferOnStream.code]
             
            
            // Verify the download retrieved the expected length of data.
            assert(downloadHandle->GetBytesTotalSize() == downloadHandle->GetBytesTransferred());

            // Verify that the length of the upload equals the download. 
            //assert(uploadHandle->GetBytesTotalSize() == downloadHandle->GetBytesTotalSize());

            // Write the buffered data to local file copy.
            Aws::OFStream storeFile(LOCAL_FILE_COPY.c_str(), Aws::OFStream::out | Aws::OFStream::trunc);
            storeFile.write((const char*)(buffer.GetUnderlyingData()),
                            static_cast<std::streamsize>(downloadHandle->GetBytesTransferred()));
            storeFile.close();

            std::cout << "File dumped to local file finished. You can verify the two files' content using md5sum." << std::endl;
            // Verify the upload file is the same as the downloaded copy. This can be done using 'md5sum' or any other file compare tool.
    }
    Aws::ShutdownAPI(options);
    return 0;
}