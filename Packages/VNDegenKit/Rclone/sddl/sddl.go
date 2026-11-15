package sddl

import "errors"

type SecurityDescriptor struct{}

var errUnsupported = errors.New("sddl: security descriptors are not supported in VNDegen's rclone")

func FromBinary(data []byte) (*SecurityDescriptor, error) {
	return nil, errUnsupported
}

func (sd *SecurityDescriptor) Binary() []byte {
	return nil
}
