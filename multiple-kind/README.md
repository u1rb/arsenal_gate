## KIND for multiple stack of docker compose

kind create cluster

Install kind and create cluster in debian

```bash
cat <<EOF > kind-config.yaml
kind: Cluster
apiVersion: kind.x-k8s.io/v1alpha4
nodes:
- role: control-plane
  extraPortMappings:
  # Reserve a range of host ports for our webservers (30000-30099)
  - containerPort: 30000
    hostPort: 30000
  - containerPort: 30001
    hostPort: 30001
  # Add as many as needed...
  - containerPort: 30099
    hostPort: 30099
EOF

kind create cluster --config kind-config.yaml
```


```bash
bash ./deploy-stacks.sh 3
```

```bash
$ kubectl get ns               
NAME                 STATUS   AGE
default              Active   4m51s
kube-node-lease      Active   4m51s
kube-public          Active   4m51s
kube-system          Active   4m51s
local-path-storage   Active   4m48s
stack-1-ns           Active   2m25s
stack-2-ns           Active   2m25s
stack-3-ns           Active   2m24s

$ kubectl get all -n stack-1-ns
NAME                                     READY   STATUS    RESTARTS   AGE
pod/stack-1-db-64ffc4798b-td7hh          1/1     Running   0          2m41s
pod/stack-1-webserver-6bc757467b-kwsjn   1/1     Running   0          2m41s

NAME                        TYPE        CLUSTER-IP      EXTERNAL-IP   PORT(S)        AGE
service/stack-1-db          ClusterIP   10.96.75.98     <none>        5432/TCP       2m41s
service/stack-1-webserver   NodePort    10.96.123.152   <none>        80:30000/TCP   2m41s

NAME                                READY   UP-TO-DATE   AVAILABLE   AGE
deployment.apps/stack-1-db          1/1     1            1           2m41s
deployment.apps/stack-1-webserver   1/1     1            1           2m41s

NAME                                           DESIRED   CURRENT   READY   AGE
replicaset.apps/stack-1-db-64ffc4798b          1         1         1       2m41s
replicaset.apps/stack-1-webserver-6bc757467b   1         1         1       2m41s
```
