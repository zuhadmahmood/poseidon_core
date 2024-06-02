#ifndef S3MANAGER_H
#define S3MANAGER_H

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/core/utils/threading/Executor.h>
#include <aws/transfer/TransferManager.h>
#include <aws/transfer/TransferHandle.h>
#include <aws/core/utils/memory/AWSMemory.h>
#include <aws/core/utils/memory/stl/AWSStreamFwd.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
#include <aws/core/utils/StringUtils.h>
#include <iostream>
#include <string>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <fstream>
#include <aws/s3/model/CreateMultipartUploadRequest.h>
#include <aws/s3/model/CompleteMultipartUploadRequest.h>
#include <aws/s3/model/AbortMultipartUploadRequest.h>
#include <aws/s3/model/UploadPartRequest.h>
#include <aws/s3/model/ListPartsRequest.h>
#include <aws/s3/model/CompletedPart.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/s3/model/BucketLocationConstraint.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include <filesystem>


namespace fs = std::filesystem;

class S3Manager {
public:
    S3Manager(const std::string& accessKey, const std::string& secretKey);
    S3Manager();

    int listBuckets();
    int putDbObject(const Aws::String fileName, const Aws::String bucketName);
    void uploadFileMultipart(const std::string& fileName, const std::string& folderName);
    int uploadTransferMan(const std::string& fileName, const std::string& folderName);
    int downloadFile(std::string fileName, std::string folderName);
    bool CreateBucket(const Aws::String &bucketName);
    bool findFileInBucket(const Aws::String& bucketName, const Aws::String& fileName);
    bool searchObjectInBucket(const Aws::String& bucketName, const Aws::String& objectKey);
    int downloadS3File(std::string path);

private:
    std::string m_accessKey;
    std::string m_secretKey;
};

#endif /* S3MANAGER_H */