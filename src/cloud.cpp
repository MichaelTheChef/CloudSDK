#include <iostream>
#include <aws/core/Aws.h>
#include <aws/ec2/EC2Client.h>
#include <aws/ec2/model/CreateVpcRequest.h>
#include <aws/ec2/model/CreateVpcResponse.h>
#include <aws/ec2/model/CreateSubnetRequest.h>
#include <aws/ec2/model/CreateSubnetResponse.h>
#include <aws/ec2/model/CreateSecurityGroupRequest.h>
#include <aws/ec2/model/CreateSecurityGroupResponse.h>
#include <aws/ec2/model/AuthorizeSecurityGroupIngressRequest.h>
#include <aws/ec2/model/RunInstancesRequest.h>
#include <aws/ec2/model/RunInstancesResponse.h>
#include <aws/ec2/model/Instance.h>

int main()
{
    Aws::SDKOptions options;
    Aws::InitAPI(options);

    {
        Aws::Auth::AWSCredentials credentials("YOUR_ACCESS_KEY", "YOUR_SECRET_KEY");
        Aws::Client::ClientConfiguration clientConfig;
        clientConfig.region = "us-west-2";

        Aws::EC2::EC2Client ec2Client(credentials, clientConfig);

        Aws::EC2::Model::CreateVpcRequest vpcRequest;
        vpcRequest.SetCidrBlock("10.0.0.0/16");

        auto vpcOutcome = ec2Client.CreateVpc(vpcRequest);
        if (!vpcOutcome.IsSuccess())
        {
            std::cout << "Failed to create VPC: " << vpcOutcome.GetError().GetMessage() << std::endl;
            return 1;
        }

        std::string vpcId = vpcOutcome.GetResult().GetVpc().GetVpcId();

        Aws::EC2::Model::CreateSubnetRequest subnetRequest;
        subnetRequest.SetVpcId(vpcId);
        subnetRequest.SetCidrBlock("10.0.0.0/24");

        auto subnetOutcome = ec2Client.CreateSubnet(subnetRequest);
        if (!subnetOutcome.IsSuccess())
        {
            std::cout << "Failed to create subnet: " << subnetOutcome.GetError().GetMessage() << std::endl;
            return 1;
        }

        std::string subnetId = subnetOutcome.GetResult().GetSubnet().GetSubnetId();

        Aws::EC2::Model::CreateSecurityGroupRequest securityGroupRequest;
        securityGroupRequest.SetGroupName("Hosting");
        securityGroupRequest.SetDescription("Hosting Security");
        securityGroupRequest.SetVpcId(vpcId);

        auto securityGroupOutcome = ec2Client.CreateSecurityGroup(securityGroupRequest);
        if (!securityGroupOutcome.IsSuccess())
        {
            std::cout << "Failed to create security group: " << securityGroupOutcome.GetError().GetMessage() << std::endl;
            return 1;
        }

        std::string securityGroupId = securityGroupOutcome.GetResult().GetGroupId();

        Aws::EC2::Model::AuthorizeSecurityGroupIngressRequest authorizeRequest;
        authorizeRequest.SetGroupId(securityGroupId);
        Aws::EC2::Model::IpPermission ipPermission;
        ipPermission.SetIpProtocol("tcp");
        ipPermission.SetFromPort(22);
        ipPermission.SetToPort(22);
        Aws::EC2::Model::IpRange ipRange;
        ipRange.SetCidrIp("0.0.0.0/0");
        ipPermission.AddIpv4Ranges(ipRange);
        authorizeRequest.AddIpPermissions(ipPermission);

        auto authorizeOutcome = ec2Client.AuthorizeSecurityGroupIngress(authorizeRequest);
        if (!authorizeOutcome.IsSuccess())
        {
            std::cout << "Failed to authorize security group ingress: " << authorizeOutcome.GetError().GetMessage() << std::endl;
            return 1;
        }

        Aws::EC2::Model::RunInstancesRequest instanceRequest;
        instanceRequest.SetImageId("AMI_IMAGE_ID");
        instanceRequest.SetInstanceType("t2.micro");
        instanceRequest.SetMinCount(1);
        instanceRequest.SetMaxCount(1);
        instanceRequest.SetSubnetId(subnetId);
        Aws::Vector<Aws::String> securityGroupIds;
        securityGroupIds.push_back(securityGroupId);
        instanceRequest.SetSecurityGroupIds(securityGroupIds);
        instanceRequest.SetKeyName("KEY_PAIR_NAME");

        auto instanceOutcome = ec2Client.RunInstances(instanceRequest);
        if (!instanceOutcome.IsSuccess())
        {
            std::cout << "Failed to launch instance: " << instanceOutcome.GetError().GetMessage() << std::endl;
            return 1;
        }

        std::string instanceId = instanceOutcome.GetResult().GetInstances()[0].GetInstanceId();

        Aws::EC2::Model::DescribeInstancesRequest describeRequest;
        describeRequest.AddInstanceIds(instanceId);

        bool isInstanceRunning = false;

        while (!isInstanceRunning)
        {
            auto describeOutcome = ec2Client.DescribeInstances(describeRequest);
            if (!describeOutcome.IsSuccess())
            {
                std::cout << "Failed to describe instance: " << describeOutcome.GetError().GetMessage() << std::endl;
                return 1;
            }

            const auto& reservations = describeOutcome.GetResult().GetReservations();
            if (!reservations.empty())
            {
                const auto& instances = reservations[0].GetInstances();
                if (!instances.empty())
                {
                    const auto& instance = instances[0];
                    if (instance.GetState().GetName() == Aws::EC2::Model::InstanceStateName::Running)
                    {
                        isInstanceRunning = true;
                    }
                }
            }

            if (!isInstanceRunning)
            {
                Aws::Utils::Threading::SleepFor(std::chrono::seconds(5));
            }
        }
    }

    Aws::ShutdownAPI(options);

    return 0;
}
