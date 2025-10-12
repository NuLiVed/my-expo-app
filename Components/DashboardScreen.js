import React from 'react';
import { View, Text, StyleSheet, Pressable, ScrollView } from 'react-native';
import { Feather } from '@expo/vector-icons';
import IndicatorCard from './IndicatorCard';
import ButtonPrimary from './ButtonPrimary';

export default function DashboardScreen({ onLogout }) {
  // Mock data with random numbers
  const humidity = (Math.random() * 20 + 50).toFixed(1); // 50-70
  const temperature = (Math.random() * 10 + 20).toFixed(1); // 20-30
  const battery = Math.floor(Math.random() * 20 + 80); // 80-100

  return (
    <ScrollView style={styles.scrollView} contentContainerStyle={styles.container}>
      <Text style={styles.header}>Dashboard</Text>

      {/* Welcome Widget */}
      <View style={styles.welcomeWidget}>
        <Text style={styles.welcomeText}>Hello! Welcome back Angelo Zamora</Text>
      </View>

      {/* Active Device Selector */}
      <View style={styles.deviceSelector}>
        <Text style={styles.deviceLabel}>Active device</Text>
        <View style={styles.deviceDropdown}>
          <Text style={styles.deviceText}>Casing 1</Text>
          <Feather name="chevron-down" size={16} color="#E67E22" />
        </View>
      </View>

      {/* Indicator Cards */}
      <View style={styles.cardsRow}>
        <IndicatorCard
          type="humidity"
          value={humidity}
          unit="%"
          icon="water"
          status="Live"
          optimal="50-70%"
          timestamp="08:50:40 PM"
        />
        <IndicatorCard
          type="temperature"
          value={temperature}
          unit="°C"
          icon="thermometer"
          status="Live"
          trend={{ value: (Math.random() * 2 - 1).toFixed(1), direction: Math.random() > 0.5 ? 'up' : 'down', color: Math.random() > 0.5 ? '#FF5722' : '#4CAF50' }}
        />
      </View>

      {/* Current Device Connected Widget */}
      <View style={styles.deviceCard}>
        <Text style={styles.deviceCardTitle}>Current Device Connected</Text>
        <View style={styles.deviceStats}>
          <Text style={styles.stat}>Temp: {temperature}°C</Text>
          <Text style={styles.stat}>Humidity: {humidity}%</Text>
          <Text style={styles.stat}>Battery: {battery}%</Text>
        </View>
        <ButtonPrimary label="Active Monitoring Device" onPress={() => {}} />
      </View>

      {/* Logs & History */}
      <View style={styles.logsSection}>
        <Text style={styles.logsTitle}>Logs & History</Text>
        <View style={styles.logItem}>
          <Text style={styles.logText}>Humidity alert: Low humidity detected in Casing 1</Text>
          <Text style={styles.logTime}>08:50:40 PM</Text>
        </View>
        <View style={styles.logItem}>
          <Text style={styles.logText}>Temperature reading: {temperature}°C</Text>
          <Text style={styles.logTime}>08:45:20 PM</Text>
        </View>
        <View style={styles.logItem}>
          <Text style={styles.logText}>Device connected: Casing 1</Text>
          <Text style={styles.logTime}>08:40:10 PM</Text>
        </View>
      </View>

      {/* Alerts */}
      <View style={styles.alertsSection}>
        <Text style={styles.alertsTitle}>Alerts</Text>
        <View style={styles.alertItem}>
          <Text style={styles.alertText}>High temperature alert in Casing 1</Text>
          <Text style={styles.alertTime}>08:55:00 PM</Text>
        </View>
        <View style={styles.alertItem}>
          <Text style={styles.alertText}>Low humidity alert in Casing 1</Text>
          <Text style={styles.alertTime}>08:50:40 PM</Text>
        </View>
        <Pressable style={styles.viewAllButton}>
          <Text style={styles.viewAllText}>View All Alerts</Text>
        </Pressable>
      </View>

      <Pressable style={styles.logoutButton} onPress={onLogout}>
        <Text style={styles.logoutText}>Logout</Text>
      </Pressable>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  scrollView: {
    flex: 1,
    backgroundColor: '#F5F5F5',
  },
  container: {
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
  welcomeWidget: {
    backgroundColor: '#FFFFFF',
    borderRadius: 10,
    padding: 16,
    marginBottom: 16,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  welcomeText: {
    fontSize: 18,
    color: '#A0522D',
    fontFamily: 'System',
  },
  deviceSelector: {
    marginBottom: 16,
  },
  deviceLabel: {
    fontSize: 16,
    color: '#A0522D',
    fontFamily: 'System',
    marginBottom: 8,
  },
  deviceDropdown: {
    backgroundColor: '#FFFFFF',
    borderRadius: 10,
    padding: 12,
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  deviceText: {
    fontSize: 16,
    color: '#A0522D',
    fontFamily: 'System',
  },
  cardsRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: 16,
  },
  deviceCard: {
    backgroundColor: '#FFFFFF',
    borderRadius: 10,
    padding: 16,
    marginBottom: 16,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  deviceCardTitle: {
    fontSize: 18,
    fontWeight: 'bold',
    color: '#A0522D',
    fontFamily: 'System',
    marginBottom: 12,
  },
  deviceStats: {
    flexDirection: 'row',
    justifyContent: 'space-around',
    marginBottom: 16,
  },
  stat: {
    fontSize: 14,
    color: '#A0522D',
    fontFamily: 'System',
  },
  logoutButton: {
    backgroundColor: '#E67E22',
    borderRadius: 20,
    paddingVertical: 12,
    paddingHorizontal: 24,
    alignItems: 'center',
    marginBottom: 20,
  },
  logoutText: {
    color: 'white',
    fontSize: 18,
    fontWeight: 'bold',
    fontFamily: 'System',
  },
  logsSection: {
    backgroundColor: '#FFFFFF',
    borderRadius: 10,
    padding: 16,
    marginBottom: 16,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  logsTitle: {
    fontSize: 18,
    fontWeight: 'bold',
    color: '#A0522D',
    fontFamily: 'System',
    marginBottom: 12,
  },
  logItem: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: 8,
    borderBottomWidth: 1,
    borderBottomColor: '#E0E0E0',
  },
  logText: {
    fontSize: 14,
    color: '#A0522D',
    fontFamily: 'System',
    flex: 1,
  },
  logTime: {
    fontSize: 12,
    color: '#A0522D',
    fontFamily: 'System',
  },
  alertsSection: {
    backgroundColor: '#FFFFFF',
    borderRadius: 10,
    padding: 16,
    marginBottom: 16,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  alertsTitle: {
    fontSize: 18,
    fontWeight: 'bold',
    color: '#A0522D',
    fontFamily: 'System',
    marginBottom: 12,
  },
  alertItem: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: 8,
    borderBottomWidth: 1,
    borderBottomColor: '#E0E0E0',
  },
  alertText: {
    fontSize: 14,
    color: '#A0522D',
    fontFamily: 'System',
    flex: 1,
  },
  alertTime: {
    fontSize: 12,
    color: '#A0522D',
    fontFamily: 'System',
  },
  viewAllButton: {
    backgroundColor: '#E67E22',
    borderRadius: 20,
    paddingVertical: 8,
    paddingHorizontal: 16,
    alignItems: 'center',
    marginTop: 12,
  },
  viewAllText: {
    color: 'white',
    fontSize: 14,
    fontWeight: 'bold',
    fontFamily: 'System',
  },
});
