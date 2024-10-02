#!/bin/bash

# Number of stacks to deploy
NUM_STACKS=${1:-3}
BASE_PORT=30000

# Check if kubectl is available
if ! command -v kubectl &> /dev/null; then
    echo "kubectl is required but not found. Please install it first."
    exit 1
fi

# Instead of creating one namespace, we'll create a namespace for each stack

for i in $(seq 1 $NUM_STACKS); do
  STACK_NAME="stack-$i"
  NAMESPACE="$STACK_NAME-ns"
  PORT=$((BASE_PORT + i - 1))
  
  echo "Creating namespace $NAMESPACE for $STACK_NAME..."
  kubectl create namespace $NAMESPACE
  
  echo "Deploying $STACK_NAME with webserver port $PORT in namespace $NAMESPACE..."
  
  # Create a ConfigMap that will store the stack ID for use by containers
  cat <<EOF | kubectl apply -f -
apiVersion: v1
kind: ConfigMap
metadata:
  name: ${STACK_NAME}-config
  namespace: ${NAMESPACE}
data:
  STACK_ID: "${i}"
EOF

  # Create the database deployment
  cat <<EOF | kubectl apply -f -
apiVersion: apps/v1
kind: Deployment
metadata:
  name: ${STACK_NAME}-db
  namespace: ${NAMESPACE}
spec:
  replicas: 1
  selector:
    matchLabels:
      app: ${STACK_NAME}-db
  template:
    metadata:
      labels:
        app: ${STACK_NAME}-db
        stack: ${STACK_NAME}
    spec:
      containers:
      - name: db
        image: postgres:14
        envFrom:
        - configMapRef:
            name: ${STACK_NAME}-config
        env:
        - name: POSTGRES_PASSWORD
          value: "password"
        - name: POSTGRES_USER
          value: "user"
        - name: POSTGRES_DB
          value: "db"
        ports:
        - containerPort: 5432
          name: postgres
        volumeMounts:
        - name: ${STACK_NAME}-db-data
          mountPath: /var/lib/postgresql/data
      volumes:
      - name: ${STACK_NAME}-db-data
        emptyDir: {}
EOF

  # Create the database service
  cat <<EOF | kubectl apply -f -
apiVersion: v1
kind: Service
metadata:
  name: ${STACK_NAME}-db
  namespace: ${NAMESPACE}
spec:
  selector:
    app: ${STACK_NAME}-db
  ports:
  - port: 5432
    targetPort: 5432
  type: ClusterIP
EOF

  # Create a ConfigMap for the custom index.html
  cat <<EOF | kubectl apply -f -
apiVersion: v1
kind: ConfigMap
metadata:
  name: ${STACK_NAME}-nginx-config
  namespace: ${NAMESPACE}
data:
  index.html: |
    <!DOCTYPE html>
    <html>
    <head>
      <title>Stack ${i} Webserver</title>
      <style>
        body {
          font-family: Arial, sans-serif;
          text-align: center;
          padding: 50px;
          background-color: #f0f8ff;
        }
        .container {
          background-color: white;
          border-radius: 10px;
          padding: 20px;
          box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
          max-width: 600px;
          margin: 0 auto;
        }
        h1 {
          color: #333;
        }
        .stack-id {
          font-size: 72px;
          font-weight: bold;
          color: #0066cc;
          margin: 20px 0;
        }
        .info {
          margin-top: 20px;
          text-align: left;
          border-top: 1px solid #eee;
          padding-top: 20px;
        }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>Docker Compose Stack Instance</h1>
        <div class="stack-id">${i}</div>
        <p>This is webserver <strong>${STACK_NAME}</strong> running on port <strong>${PORT}</strong></p>
        <div class="info">
          <p><strong>Stack ID:</strong> ${i}</p>
          <p><strong>Service Name:</strong> ${STACK_NAME}-webserver</p>
          <p><strong>DB Service:</strong> ${STACK_NAME}-db</p>
          <p><strong>Port:</strong> ${PORT}</p>
          <p><strong>Namespace:</strong> ${NAMESPACE}</p>
          <p><strong>Server Time:</strong> <span id="server-time"></span></p>
        </div>
      </div>
      <script>
        // Update server time every second
        setInterval(() => {
          document.getElementById('server-time').innerText = new Date().toLocaleString();
        }, 1000);
      </script>
    </body>
    </html>
EOF

  # Create the webserver deployment
  cat <<EOF | kubectl apply -f -
apiVersion: apps/v1
kind: Deployment
metadata:
  name: ${STACK_NAME}-webserver
  namespace: ${NAMESPACE}
spec:
  replicas: 1
  selector:
    matchLabels:
      app: ${STACK_NAME}-webserver
  template:
    metadata:
      labels:
        app: ${STACK_NAME}-webserver
        stack: ${STACK_NAME}
    spec:
      containers:
      - name: webserver
        image: nginx:latest
        envFrom:
        - configMapRef:
            name: ${STACK_NAME}-config
        env:
        - name: DB_HOST
          value: "${STACK_NAME}-db"
        - name: DB_PORT
          value: "5432"
        ports:
        - containerPort: 80
          name: http
        volumeMounts:
        - name: nginx-html
          mountPath: /usr/share/nginx/html/index.html
          subPath: index.html
      volumes:
      - name: nginx-html
        configMap:
          name: ${STACK_NAME}-nginx-config
EOF

  # Create NodePort service for the webserver
  cat <<EOF | kubectl apply -f -
apiVersion: v1
kind: Service
metadata:
  name: ${STACK_NAME}-webserver
  namespace: ${NAMESPACE}
spec:
  selector:
    app: ${STACK_NAME}-webserver
  ports:
  - port: 80
    targetPort: 80
    nodePort: ${PORT}
  type: NodePort
EOF

done

echo "All $NUM_STACKS stacks have been deployed in separate namespaces!"
echo "Access your webservers at:"
for i in $(seq 1 $NUM_STACKS); do
  STACK_NAME="stack-$i"
  NAMESPACE="$STACK_NAME-ns"
  echo "Stack $i: http://localhost:$((BASE_PORT + i - 1)) (namespace: $NAMESPACE)"
done