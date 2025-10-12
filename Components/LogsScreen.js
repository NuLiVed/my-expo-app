import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity, ScrollView } from 'react-native';
import { MaterialCommunityIcons } from '@expo/vector-icons';

export default function LogsScreen() {
  const logs = [
    { id: 1, type: 'alert', message: 'Low humidity alert for Casing 1', time: '08:50:40 PM', details: 'Humidity dropped below 50%' },
    { id: 2, type: 'telemetry', message: 'Temperature reading: 25.5°C', time: '08:45:20 PM', details: 'Device: Casing 1' },
    { id: 3, type: 'alert', message: 'Device offline: Casing 3', time: '08:40:10 PM', details: 'Connection lost' },
    { id: 4, type: 'telemetry', message: 'Humidity reading: 69.2%', time: '08:35:00 PM', details: 'Device: Casing 2' },
  ];

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.header}>Logs & History</Text>

      {logs.map((log) => (
        <TouchableOpacity
          key={log.id}
          style={[
            styles.logItem,
            log.type === 'alert' ? styles.alertLog : styles.telemetryLog
          ]}
        >
          <View style={styles.logHeader}>
            <MaterialCommunityIcons
              name={log.type === 'alert' ? 'alert' : 'chart-line'}
              size={20}
              color={log.type === 'alert' ? '#F44336' : '#2196F3'}
            />
            <Text style={styles.logType}>{log.type === 'alert' ? 'Alert' : 'Telemetry'}</Text>
            <Text style={styles.logTime}>{log.time}</Text>
          </View>
          <Text style={styles.logMessage}>{log.message}</Text>
          <Text style={styles.logDetails}>{log.details}</Text>
        </TouchableOpacity>
      ))}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#F5F5F5',
    paddingHorizontal: 16,
  },
  header: {
    fontSize: 28,
    fontWeight: 'bold',
    color: '#A0522D',
    fontFamily: 'System',
    textAlign: 'center',
    marginTop: 20,
    marginBottom: 16,
  },
  logItem: {
    borderRadius: 10,
    padding: 16,
    marginBottom: 8,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  alertLog: {
    backgroundColor: '#FFECEC',
  },
  telemetryLog: {
    backgroundColor: '#E6F4FF',
  },
  logHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 8,
  },
  logType: {
    fontSize: 14,
    fontWeight: 'bold',
    color: '#A0522D',
    fontFamily: 'System',
    marginLeft: 8,
    flex: 1,
  },
  logTime: {
    fontSize: 12,
    color: '#999',
    fontFamily: 'System',
  },
  logMessage: {
    fontSize: 16,
    color: '#A0522D',
    fontFamily: 'System',
    marginBottom: 4,
  },
  logDetails: {
    fontSize: 14,
    color: '#666',
    fontFamily: 'System',
  },
});
